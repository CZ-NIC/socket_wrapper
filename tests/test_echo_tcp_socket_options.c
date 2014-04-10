#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "config.h"
#include "torture.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#ifndef ZERO_STRUCT
#define ZERO_STRUCT(x) memset((char *)&(x), 0, sizeof(x))
#endif

static void setup_echo_srv_tcp_ipv4(void **state)
{
	torture_setup_echo_srv_tcp_ipv4(state);
}

#ifdef HAVE_IPV6
static void setup_ipv6(void **state)
{
	torture_setup_socket_dir(state);
}
#endif

static void teardown(void **state)
{
	torture_teardown_echo_srv(state);
}

static void test_sockopt_sndbuf(void **state)
{
	struct sockaddr_in sin;
	socklen_t slen = sizeof(struct sockaddr_in);
	int obufsize = 0;
	socklen_t olen = sizeof(obufsize);
	int gbufsize = 0;
	socklen_t glen = sizeof(gbufsize);
	int sbufsize = 0;
	int rc;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_int_not_equal(s, -1);

	ZERO_STRUCT(sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(torture_server_port());

	rc = inet_pton(sin.sin_family,
		       torture_server_address(AF_INET),
		       &sin.sin_addr);
	assert_int_equal(rc, 1);

	rc = connect(s, (struct sockaddr *)&sin, slen);
	assert_int_equal(rc, 0);

	rc = getsockopt(s, SOL_SOCKET, SO_SNDBUF, &obufsize, &olen);
	assert_int_equal(rc, 0);

	/* request 4k, on Linux the kernel doubles the value */
	sbufsize = 4096;
	rc = setsockopt(s, SOL_SOCKET, SO_SNDBUF, &sbufsize, sizeof(sbufsize));
	assert_int_equal(rc, 0);

	rc = getsockopt(s, SOL_SOCKET, SO_SNDBUF, &gbufsize, &glen);
	assert_int_equal(rc, 0);

	assert_true(sbufsize == gbufsize || sbufsize == gbufsize/2);

	close(s);
}

#ifdef HAVE_IPV6
static void test_bind_ipv6_only(void **state)
{
	struct addrinfo hints;
	struct addrinfo *res, *ri;
	char svc[] = "7777";
	int rc;
	int s;

	(void) state; /* unused */

	ZERO_STRUCT(hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	rc = getaddrinfo(torture_server_address(AF_INET6), svc, &hints, &res);
	assert_int_equal(rc, 0);

	for (ri = res; ri != NULL; ri = ri->ai_next) {
		int one = 1;

		s = socket(ri->ai_family,
			   ri->ai_socktype,
			   ri->ai_protocol);
		assert_int_not_equal(rc, -1);

		rc = setsockopt(s,
				IPPROTO_IPV6,
				IPV6_V6ONLY,
				(const void *)&one,
				sizeof(one));
		switch(ri->ai_family) {
		case AF_INET:
			assert_int_equal(rc, -1);

			break;
		case AF_INET6:
			assert_int_equal(rc, 0);

			rc = bind(s, ri->ai_addr, ri->ai_addrlen);
			assert_int_equal(rc, 0);

			break;
		default:
			break;
		}

		close(s);
	}
	freeaddrinfo(res);
}
#endif

int main(void) {
	int rc;

	const UnitTest tests[] = {
		unit_test_setup_teardown(test_sockopt_sndbuf, setup_echo_srv_tcp_ipv4, teardown),
#ifdef HAVE_IPV6
		unit_test_setup_teardown(test_bind_ipv6_only, setup_ipv6, teardown),
#endif
	};

	rc = run_tests(tests);

	return rc;
}
