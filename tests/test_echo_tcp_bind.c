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

static void setup_echo_srv_tcp_ipv4(void **state)
{
	torture_setup_echo_srv_tcp_ipv4(state);
}

static void setup_echo_srv_tcp_ipv6(void **state)
{
	torture_setup_echo_srv_tcp_ipv6(state);
}

static void teardown(void **state)
{
	torture_teardown_echo_srv(state);
}

static void test_bind_ipv4(void **state)
{
	struct sockaddr sa;
	socklen_t salen = sizeof(struct sockaddr);
	struct sockaddr_in sin;
	socklen_t slen = sizeof(struct sockaddr_in);
	struct sockaddr_un sun;
	socklen_t sulen = sizeof(struct sockaddr_un);
	int rc;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	/*
	 * Test various cases with family AF_UNSPEC
	 */

	/* UNSPEC, len == 1: EINVAL */

	sin = (struct sockaddr_in) {
		.sin_family = AF_UNSPEC,
	};
	rc = bind(s, (struct sockaddr *)&sin, 1);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, EINVAL);

	/* UNSPEC: EAFNOSUPPORT */

	sin = (struct sockaddr_in) {
		.sin_family = AF_UNSPEC,
	};
	rc = inet_pton(AF_INET, "127.0.0.20", &sin.sin_addr);
	assert_int_equal(rc, 1);

	rc = bind(s, (struct sockaddr *)&sin, slen);
	assert_int_equal(rc, -1);
	/* FreeBSD uses EADDRNOTAVAIL here ... */
	assert_true(errno == EAFNOSUPPORT || errno == EADDRNOTAVAIL);

	/* special case: AF_UNSPEC with INADDR_ANY: success mapped to AF_INET */

	sin = (struct sockaddr_in) {
		.sin_family = AF_UNSPEC,
	};
	assert_int_equal(sin.sin_addr.s_addr, htonl(INADDR_ANY));

	rc = bind(s, (struct sockaddr *)&sin, slen);
	assert_return_code(rc, errno);

	close(s);

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	/*
	 * Test various cases with family AF_UNIX
	 * all fail with EAFNOSUPPORT
	 */

	sa = (struct sockaddr) {
		.sa_family = AF_UNIX,
	};
	rc = bind(s, (struct sockaddr *)&sa, salen);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, EAFNOSUPPORT);

	sin = (struct sockaddr_in) {
		.sin_family = AF_UNIX,
	};
	rc = bind(s, (struct sockaddr *)&sin, slen);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, EAFNOSUPPORT);

	sun = (struct sockaddr_un) {
		.sun_family = AF_UNIX,
	};
	rc = bind(s, (struct sockaddr *)&sun, sulen);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, EAFNOSUPPORT);

#ifdef HAVE_IPV6
	/*
	 * Test with family AF_INET6 - fail
	 */

	sin = (struct sockaddr_in) {
		.sin_family = AF_INET6,
	};
	rc = bind(s, (struct sockaddr *)&sin, slen);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, EAFNOSUPPORT);
#endif

	/*
	 * Finally, success binding a new IPv4 address.
	 */
	ZERO_STRUCT(sin);
	sin.sin_family = AF_INET;

	rc = inet_pton(AF_INET, "127.0.0.20", &sin.sin_addr);
	assert_int_equal(rc, 1);

	rc = bind(s, (struct sockaddr *)&sin, slen);
	assert_return_code(rc, errno);

	ZERO_STRUCT(sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(torture_server_port());
	rc = inet_pton(AF_INET, torture_server_address(AF_INET), &sin.sin_addr);
	assert_int_equal(rc, 1);

	rc = connect(s, (struct sockaddr *)&sin, slen);
	assert_return_code(rc, errno);

	close(s);
}

static void test_bindresvport_ipv4(void **state)
{
	struct sockaddr_in sin;
	socklen_t slen = sizeof(struct sockaddr_in);
	int rc;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	ZERO_STRUCT(sin);
	sin.sin_family = AF_INET;

	rc = inet_pton(AF_INET, "127.0.0.20", &sin.sin_addr);
	assert_int_equal(rc, 1);

	rc = bindresvport(s, &sin);
	assert_return_code(rc, errno);

	ZERO_STRUCT(sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(torture_server_port());
	rc = inet_pton(AF_INET, torture_server_address(AF_INET), &sin.sin_addr);
	assert_int_equal(rc, 1);

	rc = connect(s, (struct sockaddr *)&sin, slen);
	assert_return_code(rc, errno);

	close(s);
}

static void test_bindresvport_ipv4_null(void **state)
{
	struct sockaddr_in sin;
	socklen_t slen = sizeof(struct sockaddr_in);
	int rc;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	rc = bindresvport(s, NULL);
	assert_return_code(rc, errno);

	ZERO_STRUCT(sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(torture_server_port());
	rc = inet_pton(AF_INET, torture_server_address(AF_INET), &sin.sin_addr);
	assert_int_equal(rc, 1);

	rc = connect(s, (struct sockaddr *)&sin, slen);
	assert_return_code(rc, errno);

	close(s);
}

#ifdef HAVE_IPV6
static void test_bind_on_ipv6_sock(void **state)
{
	struct sockaddr_in sin;
	socklen_t slen = sizeof(struct sockaddr_in);
	int rc;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	ZERO_STRUCT(sin);
	sin.sin_family = AF_INET;

	rc = inet_pton(AF_INET, "127.0.0.20", &sin.sin_addr);
	assert_int_equal(rc, 1);

	rc = bind(s, (struct sockaddr *)&sin, slen);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, EAFNOSUPPORT);

	close(s);
}

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

#endif /* HAVE_IPV6 */

int main(void) {
	int rc;

	const UnitTest tests[] = {
		unit_test_setup_teardown(test_bind_ipv4,
					 setup_echo_srv_tcp_ipv4,
					 teardown),
		unit_test_setup_teardown(test_bindresvport_ipv4,
					 setup_echo_srv_tcp_ipv4,
					 teardown),
		unit_test_setup_teardown(test_bindresvport_ipv4_null,
					 setup_echo_srv_tcp_ipv4,
					 teardown),
#ifdef HAVE_IPV6
		unit_test_setup_teardown(test_bind_on_ipv6_sock,
					 setup_echo_srv_tcp_ipv6,
					 teardown),
		unit_test_setup_teardown(test_bindresvport_on_ipv6_sock,
					 setup_echo_srv_tcp_ipv6,
					 teardown),
		unit_test_setup_teardown(test_bindresvport_on_ipv6_sock_null,
					 setup_echo_srv_tcp_ipv6,
					 teardown),
#endif /* HAVE_IPV6 */
	};

	rc = run_tests(tests);

	return rc;
}
