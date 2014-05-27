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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

static void test_socket_getsockname(void **state)
{
	struct sockaddr_in sin;
	socklen_t slen = sizeof(struct sockaddr_in);
	int rc;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_int_not_equal(s, -1);

	ZERO_STRUCT(sin);
	rc = getsockname(s, (struct sockaddr *)&sin, &slen);
	assert_return_code(rc, errno);
	assert_int_equal(sin.sin_family, AF_INET);
}

#ifdef HAVE_IPV6
static void test_socket_getsockname6(void **state)
{
	struct sockaddr_in6 sin6;
	socklen_t slen = sizeof(struct sockaddr_in6);
	int rc;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	assert_int_not_equal(s, -1);

	ZERO_STRUCT(sin6);
	rc = getsockname(s, (struct sockaddr *)&sin6, &slen);
	assert_return_code(rc, errno);
	assert_int_equal(sin6.sin6_family, AF_INET6);
}
#endif

int main(void) {
	int rc;

	const UnitTest tests[] = {
		unit_test(test_socket_getsockname),
#ifdef HAVE_IPV6
		unit_test(test_socket_getsockname6),
#endif
	};

	rc = run_tests(tests);

	return rc;
}
