#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "config.h"
#include "torture.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#ifdef HAVE_RPC_RPC_H
#include <rpc/rpc.h>
#endif

static int setup_echo_srv_tcp_ipv4(void **state)
{
	torture_setup_echo_srv_tcp_ipv4(state);

	return 0;
}

static int setup_echo_srv_tcp_ipv6(void **state)
{
	torture_setup_echo_srv_tcp_ipv6(state);

	return 0;
}

static int teardown(void **state)
{
	torture_teardown_echo_srv(state);

	return 0;
}

static void test_bind_ipv4(void **state)
{
	struct torture_address addr = {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};
	struct torture_address addr_in = {
		.sa_socklen = sizeof(struct sockaddr_in),
	};
	struct torture_address addr_un = {
		.sa_socklen = sizeof(struct sockaddr_un),
	};
	int rc;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	/*
	 * Test various cases with family AF_UNSPEC
	 */

	/* UNSPEC, len == 1: EINVAL */

	addr_in.sa.in = (struct sockaddr_in) {
		.sin_family = AF_UNSPEC,
	};
	rc = bind(s, &addr.sa.s, 1);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, EINVAL);

	/* UNSPEC: EAFNOSUPPORT */

	addr_in.sa.in = (struct sockaddr_in) {
		.sin_family = AF_UNSPEC,
	};
	rc = inet_pton(AF_INET, "127.0.0.20", &addr_in.sa.in.sin_addr);
	assert_int_equal(rc, 1);

	rc = bind(s, &addr_in.sa.s, addr_in.sa_socklen);
	assert_int_equal(rc, -1);
	/* FreeBSD uses EADDRNOTAVAIL here ... */
	assert_true(errno == EAFNOSUPPORT || errno == EADDRNOTAVAIL);

	/* special case: AF_UNSPEC with INADDR_ANY: success mapped to AF_INET */

	addr_in.sa.in = (struct sockaddr_in) {
		.sin_family = AF_UNSPEC,
	};
	assert_int_equal(addr_in.sa.in.sin_addr.s_addr, htonl(INADDR_ANY));

	rc = bind(s, &addr_in.sa.s, addr_in.sa_socklen);
	assert_return_code(rc, errno);

	close(s);

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	/*
	 * Test various cases with family AF_UNIX
	 * all fail with EAFNOSUPPORT
	 */

	addr.sa.ss = (struct sockaddr_storage) {
		.ss_family = AF_UNIX,
	};
	rc = bind(s, &addr.sa.s, addr.sa_socklen);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, EAFNOSUPPORT);

	addr_in.sa.in = (struct sockaddr_in) {
		.sin_family = AF_UNIX,
	};
	rc = bind(s, &addr_in.sa.s, addr_in.sa_socklen);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, EAFNOSUPPORT);

	addr_un.sa.un = (struct sockaddr_un) {
		.sun_family = AF_UNIX,
	};
	rc = bind(s, &addr_un.sa.s, addr_un.sa_socklen);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, EAFNOSUPPORT);

#ifdef HAVE_IPV6
	/*
	 * Test with family AF_INET6 - fail
	 */

	addr_in.sa.in = (struct sockaddr_in) {
		.sin_family = AF_INET6,
	};
	rc = bind(s, &addr_in.sa.s, addr_in.sa_socklen);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, EAFNOSUPPORT);
#endif

	/*
	 * Finally, success binding a new IPv4 address.
	 */
	addr_in = (struct torture_address) {
		.sa_socklen = sizeof(struct sockaddr_un),
		.sa.in = (struct sockaddr_in) {
			.sin_family = AF_INET,
		},
	};

	rc = inet_pton(AF_INET, "127.0.0.20", &addr_in.sa.in.sin_addr);
	assert_int_equal(rc, 1);

	rc = bind(s, &addr_in.sa.s, addr_in.sa_socklen);
	assert_return_code(rc, errno);

	addr_in = (struct torture_address) {
		.sa_socklen = sizeof(struct sockaddr_un),
		.sa.in = (struct sockaddr_in) {
			.sin_family = AF_INET,
			.sin_port = htons(torture_server_port()),
		},
	};

	rc = inet_pton(AF_INET,
		       torture_server_address(AF_INET),
		       &addr_in.sa.in.sin_addr);
	assert_int_equal(rc, 1);

	rc = connect(s, &addr_in.sa.s, addr_in.sa_socklen);
	assert_return_code(rc, errno);

	close(s);
}

#if 0 /* TODO */
static void test_bind_ipv4_addr_in_use(void **state)
{
	struct sockaddr_in sin, sin2;
	socklen_t slen = sizeof(struct sockaddr_in);
	int rc;
	int s, s2;

	(void) state; /* unused */

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	/*
	 * Try to bind to the same address as already bound by a
	 * different process.
	 */

	/* Without specifying the port - success */

	ZERO_STRUCT(sin);
	sin.sin_family = AF_INET;
	rc = inet_pton(AF_INET, torture_server_address(AF_INET), &sin.sin_addr);
	assert_int_equal(rc, 1);
	rc = bind(s, (struct sockaddr *)&sin, slen);
	assert_return_code(rc, errno);

	close(s);

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

#if 0
	/* specify the same port - fail with EADDRINUSE. */

	/* Not supported by socket_wrapper yet. ==> TODO! */

	ZERO_STRUCT(sin);
	sin.sin_family = AF_INET,
	sin.sin_port = htons(torture_server_port());
	rc = inet_pton(AF_INET, torture_server_address(AF_INET), &sin.sin_addr);
	assert_int_equal(rc, 1);

	rc = bind(s, (struct sockaddr *)&sin, slen);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, EADDRINUSE);
#endif

	/*
	 * Try double binding when the firs bind is with port == 0
	 */

	ZERO_STRUCT(sin);
	sin.sin_family = AF_INET;

	rc = inet_pton(AF_INET, "127.0.0.20", &sin.sin_addr);
	assert_int_equal(rc, 1);

	rc = bind(s, (struct sockaddr *)&sin, slen);
	assert_return_code(rc, errno);

	/*
	 * Open a second socket locally and try to bind to the same address.
	 */

	 /* Succeeds with port == 0 */

	s2 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	ZERO_STRUCT(sin2);
	sin2.sin_family = AF_INET;

	rc = inet_pton(AF_INET, "127.0.0.20", &sin2.sin_addr);
	assert_int_equal(rc, 1);

	rc = bind(s2, (struct sockaddr *)&sin2, slen);
	assert_return_code(rc, errno);

	close(s2);

	/* second bind with port != 0  - succeeds */

	s2 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	ZERO_STRUCT(sin2);
	sin2.sin_family = AF_INET;
	sin2.sin_port = htons(12345);

	rc = inet_pton(AF_INET, "127.0.0.20", &sin2.sin_addr);
	assert_int_equal(rc, 1);

	rc = bind(s2, (struct sockaddr *)&sin2, slen);
	assert_return_code(rc, errno);

	close(s2);
	close(s);

	/*
	 * Try double binding when the first bind is with port != 0
	 */

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	ZERO_STRUCT(sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(12345);

	rc = inet_pton(AF_INET, "127.0.0.20", &sin.sin_addr);
	assert_int_equal(rc, 1);

	rc = bind(s, (struct sockaddr *)&sin, slen);
	assert_return_code(rc, errno);

	/*
	 * Open a second socket locally and try to bind to the same address.
	 */

	 /* Succeeds with port == 0 */

	s2 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	ZERO_STRUCT(sin2);
	sin2.sin_family = AF_INET;

	rc = inet_pton(AF_INET, "127.0.0.20", &sin2.sin_addr);
	assert_int_equal(rc, 1);

	rc = bind(s2, (struct sockaddr *)&sin2, slen);
	assert_return_code(rc, errno);

	close(s2);

	/* with same port as above - fail with EADDRINUSE */

	s2 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	ZERO_STRUCT(sin2);
	sin2.sin_family = AF_INET;
	sin2.sin_port = htons(12345);

	rc = inet_pton(AF_INET, "127.0.0.20", &sin2.sin_addr);
	assert_int_equal(rc, 1);

	rc = bind(s2, (struct sockaddr *)&sin2, slen);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, EADDRINUSE);

	close(s);
}
#endif

#ifdef HAVE_BINDRESVPORT
static void test_bindresvport_ipv4(void **state)
{
	struct torture_address addr = {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};
	int rc;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	addr.sa.in.sin_family = AF_INET;

	rc = inet_pton(AF_INET, "127.0.0.20", &addr.sa.in.sin_addr);
	assert_int_equal(rc, 1);

	rc = bindresvport(s, &addr.sa.in);
	assert_return_code(rc, errno);

	addr = (struct torture_address) {
		.sa_socklen = sizeof(struct sockaddr_storage),
		.sa.in = (struct sockaddr_in) {
			.sin_family = AF_INET,
			.sin_port = htons(torture_server_port()),
		},
	};

	rc = inet_pton(AF_INET,
		       torture_server_address(AF_INET),
		       &addr.sa.in.sin_addr);
	assert_int_equal(rc, 1);

	rc = connect(s, &addr.sa.in, addr.sa_socklen);
	assert_return_code(rc, errno);

	close(s);
}

static void test_bindresvport_ipv4_null(void **state)
{
	struct torture_address addr = {
		.sa_socklen = sizeof(struct sockaddr_in),
	};
	int rc;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	rc = bindresvport(s, NULL);
	assert_return_code(rc, errno);

	addr.sa.in.sin_family = AF_INET;
	addr.sa.in.sin_port = htons(torture_server_port());
	rc = inet_pton(AF_INET, torture_server_address(AF_INET), &addr.sa.in.sin_addr);
	assert_int_equal(rc, 1);

	rc = connect(s, &addr.sa.s, addr.sa_socklen);
	assert_return_code(rc, errno);

	close(s);
}
#endif /* HAVE_BINDRESVPORT */

#ifdef HAVE_IPV6
static void test_bind_on_ipv6_sock(void **state)
{
	struct torture_address addr_in = {
		.sa_socklen = sizeof(struct sockaddr_in),
	};
	struct torture_address addr_un = {
		.sa_socklen = sizeof(struct sockaddr_un),
	};
	int rc;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	addr_un.sa.un.sun_family = AF_UNIX;
	rc = bind(s, &addr_un.sa.s, addr_un.sa_socklen);
	assert_int_equal(rc, -1);
	/* FreeBSD uses EINVAL here... */
	assert_true(errno == EAFNOSUPPORT || errno == EINVAL);

	addr_in.sa.in.sin_family = AF_INET;
	rc = bind(s, &addr_in.sa.s, addr_in.sa_socklen);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, EINVAL);

	addr_in.sa.in = (struct sockaddr_in) {
		.sin_family = AF_INET,
	};

	rc = inet_pton(AF_INET, "127.0.0.20", &addr_in.sa.in.sin_addr);
	assert_int_equal(rc, 1);

	rc = bind(s, &addr_in.sa.s, addr_in.sa_socklen);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, EINVAL);

	addr_in = (struct torture_address) {
		.sa_socklen = sizeof(struct sockaddr_in6),
		.sa.in = (struct sockaddr_in) {
			.sin_family = AF_INET,
		},
	};

	rc = bind(s, &addr_in.sa.s, addr_in.sa_socklen);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, EAFNOSUPPORT);

	close(s);
}

#ifdef HAVE_BINDRESVPORT
static void test_bindresvport_on_ipv6_sock(void **state)
{
	struct sockaddr_in sin;
	int rc;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	ZERO_STRUCT(sin);
	sin.sin_family = AF_INET;

	rc = inet_pton(AF_INET, "127.0.0.20", &sin.sin_addr);
	assert_int_equal(rc, 1);

	rc = bindresvport(s, &sin);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, EINVAL);

	close(s);
}

static void test_bindresvport_on_ipv6_sock_null(void **state)
{
	int rc;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	rc = bindresvport(s, NULL);
	assert_return_code(rc, errno);

	close(s);
}
#endif /* HAVE_BINDRESVPORT */
#endif /* HAVE_IPV6 */

int main(void) {
	int rc;

	const struct CMUnitTest tcp_bind_tests[] = {
		cmocka_unit_test_setup_teardown(test_bind_ipv4,
					 setup_echo_srv_tcp_ipv4,
					 teardown),
#if 0 /* TODO */
		cmocka_unit_test_setup_teardown(test_bind_ipv4_addr_in_use,
					 setup_echo_srv_tcp_ipv4,
					 teardown),
#endif
#ifdef HAVE_BINDRESVPORT
		cmocka_unit_test_setup_teardown(test_bindresvport_ipv4,
					 setup_echo_srv_tcp_ipv4,
					 teardown),
		cmocka_unit_test_setup_teardown(test_bindresvport_ipv4_null,
					 setup_echo_srv_tcp_ipv4,
					 teardown),
#endif /* HAVE_BINDRESVPORT */
#ifdef HAVE_IPV6
		cmocka_unit_test_setup_teardown(test_bind_on_ipv6_sock,
					 setup_echo_srv_tcp_ipv6,
					 teardown),
#ifdef HAVE_BINDRESVPORT
		cmocka_unit_test_setup_teardown(test_bindresvport_on_ipv6_sock,
					 setup_echo_srv_tcp_ipv6,
					 teardown),
		cmocka_unit_test_setup_teardown(test_bindresvport_on_ipv6_sock_null,
					 setup_echo_srv_tcp_ipv6,
					 teardown),
#endif /* HAVE_BINDRESVPORT */
#endif /* HAVE_IPV6 */
	};

	rc = cmocka_run_group_tests(tcp_bind_tests, NULL, NULL);

	return rc;
}
