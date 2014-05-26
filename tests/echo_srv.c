#include "config.h"

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

#define ECHO_SRV_IPV4 "127.0.0.10"
/* socket wrapper IPv6 prefix  fd00::5357:5fxx */
#define ECHO_SRV_IPV6 "fd00::5357:5f0a"

#define DFL_PORT    7
#define BACKLOG     5

#ifndef BUFSIZE
#define BUFSIZE     0x400000
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

#ifdef HAVE_STRUCT_MSGHDR_MSG_CONTROL
union pktinfo {
#ifdef HAVE_STRUCT_IN6_PKTINFO
	struct in6_pktinfo pkt6;
#endif
#ifdef HAVE_STRUCT_IN_PKTINFO
	struct in_pktinfo pkt4;
#endif
	char c;
};

static const char *echo_server_address(int family)
{
	switch (family) {
	case AF_INET: {
		const char *ip4 = getenv("TORTURE_SERVER_ADDRESS_IPV4");

		if (ip4 != NULL && ip4[0] != '\0') {
			return ip4;
		}

		return ECHO_SRV_IPV4;
	}
	case AF_INET6: {
		const char *ip6 = getenv("TORTURE_SERVER_ADDRESS_IPV6");

		if (ip6 != NULL && ip6[0] != '\0') {
			return ip6;
		}

		return ECHO_SRV_IPV6;
	}
	default:
		return NULL;
	}

	return NULL;
}
#endif /* HAVE_STRUCT_MSGHDR_MSG_CONTROL */

static void _assert_return_code(int rc,
				int err,
				const char * const file,
				const int line)
{
	if (rc < 0) {
		fprintf(stderr, "Fatal error: %s\n", strerror(err));
		fprintf(stderr, "%s:%d", file, line);

		abort();
	}
}
#define assert_return_code(rc, err) \
	_assert_return_code(rc, err, __FILE__, __LINE__)


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
    int i;

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

    for (i = 0; i < 3; i++) {
        fd = open("/dev/null", O_RDWR, 0);
        if (fd < 0) {
            fd = open("/dev/null", O_WRONLY, 0);
        }
        if (fd < 0) {
            ret = errno;
            perror("Can't open /dev/null");
            return ret;
        }
        if (fd != i) {
            perror("Didn't get correct fd");
            close(fd);
            return EINVAL;
        }
    }

    umask(0177);
    return 0;
}

static void set_sock_pktinfo(int sock, int family)
{
	int sockopt = 1;
	int option = 0;
	int proto = 0;
	int rc;

	switch(family) {
	case AF_INET:
#ifdef IP_PKTINFO
		proto = IPPROTO_IP;
		option = IP_PKTINFO;
#endif /* IP_PKTINFO */
		break;
#ifdef HAVE_IPV6
	case AF_INET6:
#ifdef IPV6_RECVPKTINFO
		proto = IPPROTO_IPV6;
		option = IPV6_RECVPKTINFO;
#endif
		break;
#endif /* HAVE_IPV6 */
	}

	rc = setsockopt(sock, proto, option, &sockopt, sizeof(sockopt));
	assert_return_code(rc, errno);
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

        if (ri->ai_socktype == SOCK_DGRAM) {
            set_sock_pktinfo(sock, ri->ai_family);
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
        close(s2);
        return -1;
    }

    ZERO_STRUCT(cli_ss2);
    cli_ss2_len = sizeof(cli_ss2);
    rc = getpeername(s2, (struct sockaddr *)&cli_ss2, &cli_ss2_len);
    if (rc == -1) {
        perror("getpeername");
        close(s2);
        return -1;
    }

    if (cli_ss1_len != cli_ss2_len ||
        srv_ss1_len != srv_ss2_len) {
        perror("length mismatch");
        close(s2);
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
        close(s2);
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
        close(s);
        return -1;
    }

    ZERO_STRUCT(cli_ss3);
    cli_ss3_len = sizeof(cli_ss3);
    rc = getpeername(s, (struct sockaddr *)&cli_ss3, &cli_ss3_len);
    if (rc == -1) {
        perror("getpeername");
        close(s);
        return -1;
    }

    if (cli_ss2_len != cli_ss3_len ||
        srv_ss2_len != srv_ss3_len) {
        perror("length mismatch");
        close(s);
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
        close(s);
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

static ssize_t echo_udp_recv_from_to(int sock,
				     void *buf, size_t buflen, int flags,
				     struct sockaddr *from, socklen_t *fromlen,
				     struct sockaddr *to, socklen_t *tolen)
{
	struct msghdr rmsg;
	struct iovec riov;
	ssize_t ret;

#ifdef HAVE_STRUCT_MSGHDR_MSG_CONTROL
	size_t cmlen = CMSG_LEN(sizeof(union pktinfo));
	char cmsg[cmlen];
#else
	(void)to; /* unused */
	(void)tolen; /* unused */
#endif /* HAVE_STRUCT_MSGHDR_MSG_CONTROL */

	riov.iov_base = buf;
	riov.iov_len = buflen;

	ZERO_STRUCT(rmsg);

	rmsg.msg_name = from;
	rmsg.msg_namelen = *fromlen;

	rmsg.msg_iov = &riov;
	rmsg.msg_iovlen = 1;

#ifdef HAVE_STRUCT_MSGHDR_MSG_CONTROL
	memset(cmsg, 0, cmlen);

	rmsg.msg_control = cmsg;
	rmsg.msg_controllen = cmlen;
#endif

	ret = recvmsg(sock, &rmsg, flags);
	if (ret < 0) {
		return ret;
	}
	*fromlen = rmsg.msg_namelen;

#ifdef HAVE_STRUCT_MSGHDR_MSG_CONTROL
	if (rmsg.msg_controllen > 0) {
		struct cmsghdr *cmsgptr;

		cmsgptr = CMSG_FIRSTHDR(&rmsg);
		while (cmsgptr != NULL) {
			const char *p;

#ifdef IP_PKTINFO
			if (cmsgptr->cmsg_level == IPPROTO_IP &&
					cmsgptr->cmsg_type == IP_PKTINFO) {
				char ip[INET_ADDRSTRLEN] = { 0 };
				struct in_pktinfo *pkt;
				struct sockaddr_in *sinp = (struct sockaddr_in *)to;

				pkt = (struct in_pktinfo *)CMSG_DATA(cmsgptr);

				sinp->sin_family = AF_INET;
				sinp->sin_addr = pkt->ipi_addr;
				*tolen = sizeof(struct sockaddr_in);

				p = inet_ntop(AF_INET, &sinp->sin_addr, ip, sizeof(ip));
				if (p == 0) {
					fprintf(stderr, "Failed to convert IP address");
					abort();
				}

				if (strcmp(ip, echo_server_address(AF_INET)) != 0) {
					fprintf(stderr, "Wrong IP received");
					abort();
				}
			}
#endif /* IP_PKTINFO */
#ifdef IPV6_PKTINFO
			if (cmsgptr->cmsg_level == IPPROTO_IPV6 &&
					cmsgptr->cmsg_type == IPV6_PKTINFO) {
				char ip[INET6_ADDRSTRLEN] = { 0 };
				struct in6_pktinfo *pkt6;
				struct sockaddr_in6 *sin6p = (struct sockaddr_in6 *)to;

				pkt6 = (struct in6_pktinfo *)CMSG_DATA(cmsgptr);

				sin6p->sin6_family = AF_INET6;
				sin6p->sin6_addr = pkt6->ipi6_addr;

				p = inet_ntop(AF_INET6, &sin6p->sin6_addr, ip, sizeof(ip));
				if (p == 0) {
					fprintf(stderr, "Failed to convert IP address");
					abort();
				}

				if (strcmp(ip, echo_server_address(AF_INET6)) != 0) {
					fprintf(stderr, "Wrong IP received");
					abort();
				}
			}
#endif /* IPV6_PKTINFO */
			cmsgptr = CMSG_NXTHDR(&rmsg, cmsgptr);
		}
	} else {
		fprintf(stderr, "Failed to receive pktinfo");
		abort();
	}
#endif /* HAVE_STRUCT_MSGHDR_MSG_CONTROL */

	return ret;
}

static ssize_t echo_udp_send_to_from(int sock,
				     void *buf, size_t buflen, int flags,
				     struct sockaddr *to, socklen_t tolen,
				     struct sockaddr *from, socklen_t fromlen)
{
	struct msghdr msg;
	struct iovec iov;
	ssize_t ret;

#ifdef HAVE_STRUCT_MSGHDR_MSG_CONTROL
	size_t clen = CMSG_SPACE(sizeof(union pktinfo));
	char cbuf[clen];
	struct cmsghdr *cmsgptr;
#else
	(void)from; /* unused */
	(void)fromlen; /* unused */
#endif /* !HAVE_STRUCT_MSGHDR_MSG_CONTROL */

	iov.iov_base = buf;
	iov.iov_len = buflen;

	ZERO_STRUCT(msg);

	msg.msg_name = to;
	msg.msg_namelen = tolen;

	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

#ifdef HAVE_STRUCT_MSGHDR_MSG_CONTROL
	memset(cbuf, 0, clen);

	msg.msg_control = cbuf;
	msg.msg_controllen = clen;

	cmsgptr = CMSG_FIRSTHDR(&msg);
	msg.msg_controllen = 0;

	switch (from->sa_family) {
#ifdef IP_PKTINFO
	case AF_INET: {
		struct in_pktinfo *p = (struct in_pktinfo *)CMSG_DATA(cmsgptr);
		const struct sockaddr_in *from4 = (const struct sockaddr_in *)from;

		if (fromlen != sizeof(struct sockaddr_in)) {
			break;
		}

		cmsgptr->cmsg_level = IPPROTO_IP;
		cmsgptr->cmsg_type = IP_PKTINFO;
		cmsgptr->cmsg_len = CMSG_LEN(sizeof(struct in_pktinfo));

		p->ipi_spec_dst = from4->sin_addr;

		msg.msg_controllen = CMSG_SPACE(sizeof(struct in_pktinfo));

		break;
	}
#endif /* IP_PKTINFO */
#ifdef IPV6_PKTINFO
	case AF_INET6: {
		struct in6_pktinfo *p = (struct in6_pktinfo *)CMSG_DATA(cmsgptr);
		const struct sockaddr_in6 *from6 = (const struct sockaddr_in6 *)from;

		if (fromlen != sizeof(struct sockaddr_in6)) {
			break;
		}

		cmsgptr->cmsg_level = IPPROTO_IPV6;
		cmsgptr->cmsg_type = IPV6_PKTINFO;
		cmsgptr->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));

		p->ipi6_addr = from6->sin6_addr;

		msg.msg_controllen = CMSG_SPACE(sizeof(struct in6_pktinfo));

		break;
	}
#endif /* IPV6_PKTINFO */
	default:
		break;
	}
#endif /* HAVE_STRUCT_MSGHDR_MSG_CONTROL */

	ret = sendmsg(sock, &msg, flags);

	return ret;
}

static void echo_udp(int sock)
{
    struct sockaddr_storage saddr;
    struct sockaddr_storage daddr;
    socklen_t saddrlen = sizeof(saddr);
    socklen_t daddrlen = sizeof(daddr);
    ssize_t bret;
    char buf[BUFSIZE];

    while (1) {
        bret = echo_udp_recv_from_to(sock,
                                     buf, BUFSIZE, 0,
                                     (struct sockaddr *)&saddr, &saddrlen,
                                     (struct sockaddr *)&daddr, &daddrlen);
        if (bret == -1) {
            perror("recvfrom");
            continue;
        }

        bret = echo_udp_send_to_from(sock,
                                     buf, bret, 0,
                                     (struct sockaddr *)&saddr, saddrlen,
                                     (struct sockaddr *)&daddr, daddrlen);
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
