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
	struct torture_address addr = {
		.sa_socklen = sizeof(struct sockaddr_in),
	};
	int rc;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_int_not_equal(s, -1);

	rc = getsockname(s, &addr.sa.in, &addr.sa_socklen);
	assert_return_code(rc, errno);
	assert_int_equal(addr.sa.in.sin_family, AF_INET);
}

#ifdef HAVE_IPV6
static void test_socket_getsockname6(void **state)
{
	struct torture_address addr = {
		.sa_socklen = sizeof(struct sockaddr_in),
	};
	int rc;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	assert_int_not_equal(s, -1);

	rc = getsockname(s, &addr.sa.s, &addr.sa_socklen);
	assert_return_code(rc, errno);
	assert_int_equal(addr.sa.in6.sin6_family, AF_INET6);
}
#endif

int main(void) {
	int rc;

	const struct CMUnitTest getsockname_tests[] = {
		cmocka_unit_test(test_socket_getsockname),
#ifdef HAVE_IPV6
		cmocka_unit_test(test_socket_getsockname6),
#endif
	};

	rc = cmocka_run_group_tests(getsockname_tests, NULL, NULL);

	return rc;
}
