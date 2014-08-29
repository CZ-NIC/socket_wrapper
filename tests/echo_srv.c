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

struct torture_address {
	socklen_t sa_socklen;
	union {
		struct sockaddr s;
		struct sockaddr_in in;
#ifdef HAVE_IPV6
		struct sockaddr_in6 in6;
#endif
		struct sockaddr_storage ss;
	} sa;
};

struct echo_srv_opts {
    int port;
    int socktype;
    bool daemon;
    char *bind;
    const char *pidfile;
};

#ifdef HAVE_STRUCT_MSGHDR_MSG_CONTROL

#if defined(IP_PKTINFO) || defined(IP_RECVDSTADDR) || defined(IPV6_PKTINFO)
union pktinfo {
#ifdef HAVE_STRUCT_IN6_PKTINFO
	struct in6_pktinfo pkt6;
#endif
#ifdef HAVE_STRUCT_IN_PKTINFO
	struct in_pktinfo pkt4;
#elif defined(IP_RECVDSTADDR)
	struct in_addr pkt4;
#endif
	char c;
};

#define HAVE_UNION_PKTINFO 1
#endif /* IP_PKTINFO || IP_RECVDSTADDR || IPV6_PKTINFO */

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

static int become_daemon(void)
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
		proto = IPPROTO_IP;
#ifdef IP_PKTINFO
		option = IP_PKTINFO;
#elif IP_RECVDSTADDR
		option = IP_RECVDSTADDR;
#else
		return;
#endif /* IP_PKTINFO */
		break;
#ifdef HAVE_IPV6
#ifdef IPV6_RECVPKTINFO
	case AF_INET6:
		proto = IPPROTO_IPV6;
		option = IPV6_RECVPKTINFO;
		break;
#endif /* IPV6_RECVPKTINFO */
#endif /* HAVE_IPV6 */
	default:
		return;
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
    struct torture_address cli_addr1 = {
        .sa_socklen = sizeof(struct sockaddr_storage),
    };
    struct torture_address srv_addr1 = {
        .sa_socklen = sizeof(struct sockaddr_storage),
    };

    struct torture_address cli_addr2 = {
        .sa_socklen = sizeof(struct sockaddr_storage),
    };
    struct torture_address srv_addr2 = {
        .sa_socklen = sizeof(struct sockaddr_storage),
    };

    struct torture_address cli_addr3 = {
        .sa_socklen = sizeof(struct sockaddr_storage),
    };
    struct torture_address srv_addr3 = {
        .sa_socklen = sizeof(struct sockaddr_storage),
    };

    int s2;
    int rc;

    rc = getsockname(s, &srv_addr1.sa.s, &srv_addr1.sa_socklen);
    if (rc == -1) {
        perror("getsockname");
        return -1;
    }

    rc = getpeername(s, &cli_addr1.sa.s, &cli_addr1.sa_socklen);
    if (rc == -1) {
        perror("getpeername");
        return -1;
    }

    if (cli_addr1.sa.ss.ss_family != srv_addr1.sa.ss.ss_family) {
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

    rc = getsockname(s2, &srv_addr2.sa.s, &srv_addr2.sa_socklen);
    if (rc == -1) {
        perror("getsockname");
        close(s2);
        return -1;
    }

    rc = getpeername(s2, &cli_addr2.sa.s, &cli_addr2.sa_socklen);
    if (rc == -1) {
        perror("getpeername");
        close(s2);
        return -1;
    }

    if (cli_addr1.sa_socklen != cli_addr2.sa_socklen ||
        srv_addr1.sa_socklen != srv_addr2.sa_socklen) {
        perror("length mismatch");
        close(s2);
        return -1;
    }

    switch(cli_addr1.sa.ss.ss_family) {
    case AF_INET: {
        rc = memcmp(&cli_addr1.sa.in, &cli_addr2.sa.in, sizeof(struct sockaddr_in));
        if (rc != 0) {
            perror("client mismatch");
        }

        rc = memcmp(&srv_addr1.sa.in, &srv_addr2.sa.in, sizeof(struct sockaddr_in));
        if (rc != 0) {
            perror("server mismatch");
        }
        break;
    }
    case AF_INET6: {
        rc = memcmp(&cli_addr1.sa.in6, &cli_addr2.sa.in6, sizeof(struct sockaddr_in6));
        if (rc != 0) {
            perror("client mismatch");
        }

        rc = memcmp(&srv_addr1.sa.in6, &srv_addr2.sa.in6, sizeof(struct sockaddr_in6));
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

    rc = getsockname(s, &srv_addr3.sa.s, &srv_addr3.sa_socklen);
    if (rc == -1) {
        perror("getsockname");
        close(s);
        return -1;
    }

    rc = getpeername(s, &cli_addr3.sa.s, &cli_addr3.sa_socklen);
    if (rc == -1) {
        perror("getpeername");
        close(s);
        return -1;
    }

    if (cli_addr2.sa_socklen != cli_addr3.sa_socklen ||
        srv_addr2.sa_socklen != srv_addr3.sa_socklen) {
        perror("length mismatch");
        close(s);
        return -1;
    }

    switch(cli_addr2.sa.ss.ss_family) {
    case AF_INET: {
        rc = memcmp(&cli_addr1.sa.in, &cli_addr2.sa.in, sizeof(struct sockaddr_in));
        if (rc != 0) {
            perror("client mismatch");
        }

        rc = memcmp(&srv_addr1.sa.in, &srv_addr2.sa.in, sizeof(struct sockaddr_in));
        if (rc != 0) {
            perror("server mismatch");
        }
        break;
    }
    case AF_INET6: {
        rc = memcmp(&cli_addr1.sa.in6, &cli_addr2.sa.in6, sizeof(struct sockaddr_in6));
        if (rc != 0) {
            perror("client mismatch");
        }

        rc = memcmp(&srv_addr1.sa.in6, &srv_addr2.sa.in6, sizeof(struct sockaddr_in6));
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
    struct torture_address addr = {
        .sa_socklen = sizeof(struct sockaddr_storage),
    };

    char buf[BUFSIZE];
    ssize_t bret;

    int client_sock = -1;
    int s;

    s = accept(sock, &addr.sa.s, &addr.sa_socklen);
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

#if defined(HAVE_STRUCT_MSGHDR_MSG_CONTROL) && defined(HAVE_UNION_PKTINFO)
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

#if defined(HAVE_STRUCT_MSGHDR_MSG_CONTROL) && defined(HAVE_UNION_PKTINFO)
	memset(cmsg, 0, cmlen);

	rmsg.msg_control = cmsg;
	rmsg.msg_controllen = cmlen;
#endif

	ret = recvmsg(sock, &rmsg, flags);
	if (ret < 0) {
		return ret;
	}
	*fromlen = rmsg.msg_namelen;

#if defined(HAVE_STRUCT_MSGHDR_MSG_CONTROL) && defined(HAVE_UNION_PKTINFO)
	if (rmsg.msg_controllen > 0) {
		struct cmsghdr *cmsgptr;

		cmsgptr = CMSG_FIRSTHDR(&rmsg);
		while (cmsgptr != NULL) {
			const char *p;

#if defined(IP_PKTINFO) && defined(HAVE_STRUCT_IN_PKTINFO)
			if (cmsgptr->cmsg_level == IPPROTO_IP &&
					cmsgptr->cmsg_type == IP_PKTINFO) {
				char ip[INET_ADDRSTRLEN] = { 0 };
				struct sockaddr_in *sinp = (struct sockaddr_in *)to;
				struct in_pktinfo *pkt;
				void *cmsg_cast_ptr = CMSG_DATA(cmsgptr);

				pkt = (struct in_pktinfo *)cmsg_cast_ptr;

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
#ifdef IP_RECVDSTADDR
			if (cmsgptr->cmsg_level == IPPROTO_IP &&
			    cmsgptr->cmsg_type == IP_RECVDSTADDR) {
				char ip[INET_ADDRSTRLEN] = { 0 };
				struct sockaddr_in *sinp = (struct sockaddr_in *)to;
				struct in_addr *addr;
				void *cmsg_cast_ptr = CMSG_DATA(cmsgptr);

				addr = (struct in_addr *)cmsg_cast_ptr;

				sinp->sin_family = AF_INET;
				sinp->sin_addr = *addr;
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
#endif /* IP_RECVDSTADDR */
#if defined(IPV6_PKTINFO) && defined(HAVE_STRUCT_IN6_PKTINFO)
			if (cmsgptr->cmsg_level == IPPROTO_IPV6 &&
					cmsgptr->cmsg_type == IPV6_PKTINFO) {
				char ip[INET6_ADDRSTRLEN] = { 0 };
				struct in6_pktinfo *pkt6;
				struct sockaddr_in6 *sin6p = (struct sockaddr_in6 *)to;
				void *cmsg_cast_ptr = CMSG_DATA(cmsgptr);

				pkt6 = (struct in6_pktinfo *)cmsg_cast_ptr;

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
#endif /* HAVE_STRUCT_MSGHDR_MSG_CONTROL && HAVE_UNION_PKTINFO */

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

#if defined(HAVE_STRUCT_MSGHDR_MSG_CONTROL) && defined(HAVE_UNION_PKTINFO)
	size_t clen = CMSG_SPACE(sizeof(union pktinfo));
	char cbuf[clen];
	struct cmsghdr *cmsgptr;
#else
	(void)from; /* unused */
	(void)fromlen; /* unused */
#endif /* HAVE_STRUCT_MSGHDR_MSG_CONTROL && HAVE_UNION_PKTINFO */

	iov.iov_base = buf;
	iov.iov_len = buflen;

	ZERO_STRUCT(msg);

	msg.msg_name = to;
	msg.msg_namelen = tolen;

	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

#if defined(HAVE_STRUCT_MSGHDR_MSG_CONTROL) && defined(HAVE_UNION_PKTINFO)
	memset(cbuf, 0, clen);

	msg.msg_control = cbuf;
	msg.msg_controllen = clen;

	cmsgptr = CMSG_FIRSTHDR(&msg);
	msg.msg_controllen = 0;

	switch (from->sa_family) {
#if defined(IP_PKTINFO) || defined(IP_SENDSRCADDR)
	case AF_INET: {
		void *cmsg_cast_ptr = CMSG_DATA(cmsgptr);
#ifdef IP_PKTINFO
		struct in_pktinfo *p = (struct in_pktinfo *)cmsg_cast_ptr;
#elif defined(IP_SENDSRCADDR)
		struct in_addr *p = (struct in_addr *)cmsg_cast_ptr;
#endif
		const struct sockaddr_in *from4 = (const struct sockaddr_in *)from;

		if (fromlen != sizeof(struct sockaddr_in)) {
			break;
		}

		cmsgptr->cmsg_level = IPPROTO_IP;
#ifdef IP_PKTINFO
		cmsgptr->cmsg_type = IP_PKTINFO;
		p->ipi_spec_dst = from4->sin_addr;
#elif defined(IP_SENDSRCADDR)
		cmsgptr->cmsg_type = IP_SENDSRCADDR;
		*p = from4->sin_addr;
#endif
		cmsgptr->cmsg_len = CMSG_LEN(sizeof(*p));

		msg.msg_controllen = CMSG_SPACE(sizeof(*p));

		break;
	}
#endif /* IP_PKTINFO || IP_SENDSRCADDR */
#ifdef IPV6_PKTINFO
	case AF_INET6: {
        void *cast_ptr = CMSG_DATA(cmsgptr);
		struct in6_pktinfo *p = (struct in6_pktinfo *)cast_ptr;
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
#endif /* HAVE_STRUCT_MSGHDR_MSG_CONTROL && HAVE_UNION_PKTINFO */

	ret = sendmsg(sock, &msg, flags);

	return ret;
}

static void echo_udp(int sock)
{
    struct torture_address saddr = {
        .sa_socklen = sizeof(struct sockaddr_storage),
    };
    struct torture_address daddr = {
        .sa_socklen = sizeof(struct sockaddr_storage),
    };
    ssize_t bret;
    char buf[BUFSIZE];

    while (1) {
        bret = echo_udp_recv_from_to(sock,
                                     buf,
                                     BUFSIZE,
                                     0,
                                     &saddr.sa.s,
                                     &saddr.sa_socklen,
                                     &daddr.sa.s,
                                     &daddr.sa_socklen);
        if (bret == -1) {
            perror("recvfrom");
            continue;
        }

        bret = echo_udp_send_to_from(sock,
                                     buf,
                                     bret,
                                     0,
                                     &saddr.sa.s,
                                     saddr.sa_socklen,
                                     &daddr.sa.s,
                                     daddr.sa_socklen);
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
        ret = become_daemon();
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

    if (opts.daemon && opts.pidfile != NULL) {
        ret = pidfile(opts.pidfile);
        if (ret != 0) {
            fprintf(stderr, "Cannot create pidfile %s: %s\n",
                    opts.pidfile, strerror(ret));
            goto done;
        }
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
