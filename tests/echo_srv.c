#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>

#ifndef PIDFILE
#define PIDFILE     "echo_srv.pid"
#endif  /* PIDFILE */

#define DFL_PORT    7
#define BACKLOG     5

#ifndef BUFSIZE
#define BUFSIZE     4194304
#endif /* BUFSIZE */

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
    char pid_str[32];

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

    memset(pid_str, 0, sizeof(pid_str));
    snprintf(pid_str, sizeof(pid_str) -1, "%u\n", (unsigned int) getpid());
    write(fd, pid_str, strlen(pid_str));

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
            perror("listen");
            return ret;
        }
    }

    *_sock = sock;
    return 0;
}

static void echo_tcp(int sock)
{
    int client_sock;
    struct sockaddr_storage css;
    socklen_t addrlen = sizeof(css);
    ssize_t bret;
    char buf[BUFSIZE];

    client_sock = accept(sock, (struct sockaddr *) &css, &addrlen);
    if (client_sock == -1) {
        perror("accept");
        return;
    }

    while (1) {
        bret = recv(client_sock, buf, BUFSIZE, 0);
        if (bret == -1) {
            close(client_sock);
            perror("recv");
            continue;
        } else if (bret == 0) {
            break;
        }

        bret = send(client_sock, buf, bret, 0);
        if (bret == -1) {
            close(client_sock);
            perror("send");
            continue;
        }
    }

    close(client_sock);
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
    int sock;
    struct echo_srv_opts opts;
    int opt;
    int optindex;
    static struct option long_options[] = {
        {"tcp",         no_argument, 0,  't' },
        {"udp",         no_argument, 0,  'u' },
        {"bind-addr",   required_argument, 0,  'b' },
        {"port",        required_argument, 0,  'p' },
        {"daemon",      no_argument, 0,  'D' },
        {"pid",         required_argument, 0,  0 },
        {0,             0,                 0,  0 }
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
