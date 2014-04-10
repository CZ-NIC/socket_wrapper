#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <resolv.h>

#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifndef PIDFILE
#define PIDFILE     "echo_srv.pid"
#endif  /* PIDFILE */

#define DFL_PORT    7
#define BACKLOG     5

#ifndef BUFSIZE
#define BUFSIZE     4194304
#endif /* BUFSIZE */

#ifndef discard_const
#define discard_const(ptr) ((void *)((uintptr_t)(ptr)))
#endif

#ifndef discard_const_p
#define discard_const_p(type, ptr) ((type *)discard_const(ptr))
#endif

#ifndef ZERO_STRUCT
#define ZERO_STRUCT(x) memset((char *)&(x), 0, sizeof(x))
#endif

struct echo_srv_opts {
    int port;
    int socktype;
    bool daemon;
    char *bind;
    const char *pidfile;
};

static int pidfile(const char *path)
{
    int err;
    int fd;
    char pid_str[32] = { 0 };
    ssize_t nwritten;
    size_t len;

    fd = open(path, O_RDONLY, 0644);
    err = errno;
    if (fd != -1) {
        close(fd);
        return EEXIST;
    } else if (err != ENOENT) {
        return err;
    }

    fd = open(path, O_CREAT | O_WRONLY | O_EXCL, 0644);
    err = errno;
    if (fd == -1) {
        return err;
    }

    snprintf(pid_str, sizeof(pid_str) -1, "%u\n", (unsigned int) getpid());
    len = strlen(pid_str);

    nwritten = write(fd, pid_str, len);
    close(fd);
    if (nwritten != (ssize_t)len) {
        return EIO;
    }

    return 0;
}

static int become_daemon(struct echo_srv_opts *opts)
{
    int ret;
    pid_t child_pid;
    int fd;

    if (getppid() == 1) {
        return 0;
    }

    child_pid = fork();
    if (child_pid == -1) {
        ret = errno;
        perror("fork");
        return ret;
    } else if (child_pid > 0) {
        exit(0);
    }

    /* If a working directory was defined, go there */
#ifdef WORKING_DIR
    chdir(WORKING_DIR);
#endif

    ret = pidfile(opts->pidfile);
    if (ret != 0) {
        fprintf(stderr, "Cannot create pidfile %s: %s\n",
                        opts->pidfile, strerror(ret));
        return ret;
    }

    ret = setsid();
    if (ret == -1) {
        ret = errno;
        perror("setsid");
        return ret;
    }

    for (fd = getdtablesize(); fd >= 0; --fd) {
        close(fd);
    }

    fd = open("/dev/null", O_RDWR);
    if (fd == -1) {
        ret = errno;
        perror("open");
        return ret;
    }
    dup(fd);
    dup(fd);

    umask(0177);
    return 0;
}

/* Returns 0 on success, errno on failure. If successful,
 * sock is a ready to use socket */
static int setup_srv(struct echo_srv_opts *opts, int *_sock)
{
    struct addrinfo hints;
    struct addrinfo *res, *ri;
    char svc[6];
    int ret;
    int sock;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = opts->socktype;
    hints.ai_flags = AI_PASSIVE;

    snprintf(svc, sizeof(svc), "%d", opts->port);

    ret = getaddrinfo(opts->bind, svc, &hints, &res);
    if (ret != 0) {
        return errno;
    }

    for (ri = res; ri != NULL; ri = ri->ai_next) {
        sock = socket(ri->ai_family, ri->ai_socktype,
                      ri->ai_protocol);
        if (sock == -1) {
            ret = errno;
            freeaddrinfo(res);
            perror("socket");
            return ret;
        }

        ret = bind(sock, ri->ai_addr, ri->ai_addrlen);
        if (ret == 0) {
            break;
        }

        close(sock);
    }
    freeaddrinfo(res);

    if (ri == NULL) {
        fprintf(stderr, "Could not bind\n");
        return EFAULT;
    }

    if (opts->socktype == SOCK_STREAM) {
        ret = listen(sock, BACKLOG);
        if (ret == -1) {
            ret = errno;
            close(sock);
            perror("listen");
            return ret;
        }
    }

    *_sock = sock;
    return 0;
}

static int socket_dup(int s)
{
    struct sockaddr_storage cli_ss1;
    socklen_t cli_ss1_len;
    struct sockaddr_storage srv_ss1;
    socklen_t srv_ss1_len;

    struct sockaddr_storage cli_ss2;
    socklen_t cli_ss2_len;
    struct sockaddr_storage srv_ss2;
    socklen_t srv_ss2_len;

    struct sockaddr_storage cli_ss3;
    socklen_t cli_ss3_len;
    struct sockaddr_storage srv_ss3;
    socklen_t srv_ss3_len;

    int s2;
    int rc;

    ZERO_STRUCT(srv_ss1);
    srv_ss1_len = sizeof(srv_ss1);
    rc = getsockname(s, (struct sockaddr *)&srv_ss1, &srv_ss1_len);
    if (rc == -1) {
        perror("getsockname");
        return -1;
    }

    ZERO_STRUCT(cli_ss1);
    cli_ss1_len = sizeof(cli_ss1);
    rc = getpeername(s, (struct sockaddr *)&cli_ss1, &cli_ss1_len);
    if (rc == -1) {
        perror("getpeername");
        return -1;
    }

    if (cli_ss1.ss_family != srv_ss1.ss_family) {
        perror("client/server family mismatch");
        return -1;
    }

    /* Test dup */
    s2 = dup(s);
    if (s2 == -1) {
        perror("dup");
        return -1;
    }
    close(s);

    ZERO_STRUCT(srv_ss2);
    srv_ss2_len = sizeof(srv_ss2);
    rc = getsockname(s2, (struct sockaddr *)&srv_ss2, &srv_ss2_len);
    if (rc == -1) {
        perror("getsockname");
        return -1;
    }

    ZERO_STRUCT(cli_ss2);
    cli_ss2_len = sizeof(cli_ss2);
    rc = getpeername(s2, (struct sockaddr *)&cli_ss2, &cli_ss2_len);
    if (rc == -1) {
        perror("getpeername");
        return -1;
    }

    if (cli_ss1_len != cli_ss2_len ||
        srv_ss1_len != srv_ss2_len) {
        perror("length mismatch");
        return -1;
    }

    switch(cli_ss1.ss_family) {
    case AF_INET: {
        struct sockaddr_in *cli_sinp1 = (struct sockaddr_in *)&cli_ss1;
        struct sockaddr_in *cli_sinp2 = (struct sockaddr_in *)&cli_ss2;

        struct sockaddr_in *srv_sinp1 = (struct sockaddr_in *)&srv_ss1;
        struct sockaddr_in *srv_sinp2 = (struct sockaddr_in *)&srv_ss2;

        rc = memcmp(cli_sinp1, cli_sinp2, sizeof(struct sockaddr_in));
        if (rc != 0) {
            perror("client mismatch");
        }

        rc = memcmp(srv_sinp1, srv_sinp2, sizeof(struct sockaddr_in));
        if (rc != 0) {
            perror("server mismatch");
        }
        break;
    }
    case AF_INET6: {
        struct sockaddr_in6 *cli_sinp1 = (struct sockaddr_in6 *)&cli_ss1;
        struct sockaddr_in6 *cli_sinp2 = (struct sockaddr_in6 *)&cli_ss2;

        struct sockaddr_in6 *srv_sinp1 = (struct sockaddr_in6 *)&srv_ss1;
        struct sockaddr_in6 *srv_sinp2 = (struct sockaddr_in6 *)&srv_ss2;

        rc = memcmp(cli_sinp1, cli_sinp2, sizeof(struct sockaddr_in6));
        if (rc != 0) {
            perror("client mismatch");
        }

        rc = memcmp(srv_sinp1, srv_sinp2, sizeof(struct sockaddr_in6));
        if (rc != 0) {
            perror("server mismatch");
        }
        break;
    }
    default:
        perror("family mismatch");
        return -1;
    }

    /* Test dup2 */
    s = dup2(s2, s);
    close(s2);
    if (s == -1) {
        perror("dup");
        return -1;
    }

    ZERO_STRUCT(srv_ss3);
    srv_ss3_len = sizeof(srv_ss3);
    rc = getsockname(s, (struct sockaddr *)&srv_ss3, &srv_ss3_len);
    if (rc == -1) {
        perror("getsockname");
        return -1;
    }

    ZERO_STRUCT(cli_ss3);
    cli_ss3_len = sizeof(cli_ss3);
    rc = getpeername(s, (struct sockaddr *)&cli_ss3, &cli_ss3_len);
    if (rc == -1) {
        perror("getpeername");
        return -1;
    }

    if (cli_ss2_len != cli_ss3_len ||
        srv_ss2_len != srv_ss3_len) {
        perror("length mismatch");
        return -1;
    }

    switch(cli_ss2.ss_family) {
    case AF_INET: {
        struct sockaddr_in *cli_sinp1 = (struct sockaddr_in *)&cli_ss2;
        struct sockaddr_in *cli_sinp2 = (struct sockaddr_in *)&cli_ss3;

        struct sockaddr_in *srv_sinp1 = (struct sockaddr_in *)&srv_ss2;
        struct sockaddr_in *srv_sinp2 = (struct sockaddr_in *)&srv_ss3;

        rc = memcmp(cli_sinp1, cli_sinp2, sizeof(struct sockaddr_in));
        if (rc != 0) {
            perror("client mismatch");
        }

        rc = memcmp(srv_sinp1, srv_sinp2, sizeof(struct sockaddr_in));
        if (rc != 0) {
            perror("server mismatch");
        }
        break;
    }
    case AF_INET6: {
        struct sockaddr_in6 *cli_sinp1 = (struct sockaddr_in6 *)&cli_ss2;
        struct sockaddr_in6 *cli_sinp2 = (struct sockaddr_in6 *)&cli_ss3;

        struct sockaddr_in6 *srv_sinp1 = (struct sockaddr_in6 *)&srv_ss2;
        struct sockaddr_in6 *srv_sinp2 = (struct sockaddr_in6 *)&srv_ss3;

        rc = memcmp(cli_sinp1, cli_sinp2, sizeof(struct sockaddr_in6));
        if (rc != 0) {
            perror("client mismatch");
        }

        rc = memcmp(srv_sinp1, srv_sinp2, sizeof(struct sockaddr_in6));
        if (rc != 0) {
            perror("server mismatch");
        }
        break;
    }
    default:
        perror("family mismatch");
        return -1;
    }

    return s;
}

static void echo_tcp(int sock)
{
    struct sockaddr_storage css;
    socklen_t addrlen = sizeof(css);

    char buf[BUFSIZE];
    ssize_t bret;

    int client_sock = -1;
    int s;

    s = accept(sock, (struct sockaddr *)&css, &addrlen);
    if (s == -1) {
        perror("accept");
	goto done;
    }

    client_sock = socket_dup(s);
    if (client_sock == -1) {
        perror("socket_dup");
	goto done;
    }

    /* Start ping pong */
    while (1) {
        bret = recv(client_sock, buf, BUFSIZE, 0);
        if (bret == -1) {
            perror("recv");
            goto done;
        } else if (bret == 0) {
            break;
        }

        bret = send(client_sock, buf, bret, 0);
        if (bret == -1) {
            perror("send");
            goto done;
        }
    }

done:
    if (client_sock != -1) {
	    close(client_sock);
    }
}

static void echo_udp(int sock)
{
    struct sockaddr_storage css;
    socklen_t addrlen = sizeof(css);
    ssize_t bret;
    char buf[BUFSIZE];

    while (1) {
        bret = recvfrom(sock, buf, BUFSIZE, 0,
                        (struct sockaddr *) &css, &addrlen);
        if (bret == -1) {
            perror("recvfrom");
            continue;
        }

        bret = sendto(sock, buf, bret, 0,
                      (struct sockaddr *) &css, addrlen);
        if (bret == -1) {
            perror("sendto");
            continue;
        }
    }
}

static void echo(int sock, struct echo_srv_opts *opts)
{
    switch (opts->socktype) {
        case SOCK_STREAM:
            echo_tcp(sock);
            return;
        case SOCK_DGRAM:
            echo_udp(sock);
            return;
        default:
            fprintf(stderr, "Unsupported protocol\n");
            return;
    }
}

int main(int argc, char **argv)
{
    int ret;
    int sock = -1;
    struct echo_srv_opts opts;
    int opt;
    int optindex;
    static struct option long_options[] = {
        { discard_const_p(char, "tcp"),         no_argument,		0,  't' },
        { discard_const_p(char, "udp"),         no_argument,		0,  'u' },
        { discard_const_p(char, "bind-addr"),   required_argument,	0,  'b' },
        { discard_const_p(char, "port"),        required_argument,	0,  'p' },
        { discard_const_p(char, "daemon"),      no_argument,		0,  'D' },
        { discard_const_p(char, "pid"),         required_argument,	0,  0 },
        {0,             0,						0,  0 }
    };

    opts.port = DFL_PORT;
    opts.socktype = SOCK_STREAM;
    opts.bind = NULL;
    opts.pidfile = PIDFILE;
    opts.daemon = false;

    while ((opt = getopt_long(argc, argv, "Dutp:b:",
                              long_options, &optindex)) != -1) {
        switch (opt) {
            case 0:
                if (optindex == 5) {
                    opts.pidfile = optarg;
                }
                break;
            case 'p':
                opts.port = atoi(optarg);
                break;
            case 'b':
                opts.bind = optarg;
                break;
            case 'u':
                opts.socktype = SOCK_DGRAM;
                break;
            case 't':
                opts.socktype = SOCK_STREAM;
                break;
            case 'D':
                opts.daemon = true;
                break;
            default: /* '?' */
                fprintf(stderr, "Usage: %s [-p port] [-u] [-t] [-b bind_addr] " \
                                "[-D] [--pid pidfile]\n"
                                "-t makes the server listen on TCP\n"
                                "-u makes the server listen on UDP\n"
                                "-D tells the server to become a deamon and " \
                                "write a PIDfile\n"
                                "The default port is 7, the default PIDfile is " \
                                "echo_srv.pid in the current directory\n",
                                argv[0]);
                ret = 1;
                goto done;
        }
    }

    if (opts.daemon) {
        ret = become_daemon(&opts);
        if (ret != 0) {
            fprintf(stderr, "Cannot become daemon: %s\n", strerror(ret));
            goto done;
        }
    }

    ret = setup_srv(&opts, &sock);
    if (ret != 0) {
        fprintf(stderr, "Cannot setup server: %s\n", strerror(ret));
        goto done;
    }

    echo(sock, &opts);
    close(sock);

    if (opts.daemon) {
        unlink(opts.pidfile);
    }

    ret = 0;
done:
    return ret;
}
