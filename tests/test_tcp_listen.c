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

static int setup(void **state)
{
	torture_setup_socket_dir(state);

	return 0;
}

static int teardown(void **state)
{
	torture_teardown_socket_dir(state);

	return 0;
}

static void test_listen_unbound_ipv4(void **state)
{
	struct torture_address addr = {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};
	int rc;
	int s1;
	int s2;

	(void) state; /* unused */

	s1 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s1, errno);

	rc = listen(s1, SOMAXCONN);
	assert_return_code(rc, errno);

	rc = getsockname(s1, &addr.sa.s, &addr.sa_socklen);
	assert_return_code(rc, errno);
	assert_int_equal(addr.sa_socklen, sizeof(struct sockaddr_in));
	assert_in_range(ntohs(addr.sa.in.sin_port), 1024, 65535);

	s2 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s2, errno);

	rc = connect(s2, &addr.sa.s, addr.sa_socklen);
	assert_return_code(rc, errno);

	close(s1);
	close(s2);
}

#ifdef HAVE_IPV6
static void test_listen_unbound_ipv6(void **state)
{
	struct torture_address addr = {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};
	int rc;
	int s1;
	int s2;

	(void) state; /* unused */

	s1 = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s1, errno);

	rc = listen(s1, SOMAXCONN);
	assert_return_code(rc, errno);

	rc = getsockname(s1, &addr.sa.s, &addr.sa_socklen);
	assert_return_code(rc, errno);
	assert_int_equal(addr.sa_socklen, sizeof(struct sockaddr_in6));
	assert_in_range(ntohs(addr.sa.in6.sin6_port), 1024, 65535);

	s2 = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s2, errno);

	rc = connect(s2, &addr.sa.s, addr.sa_socklen);
	assert_return_code(rc, errno);

	close(s1);
	close(s2);
}
#endif /* HAVE_IPV6 */

int main(void) {
	int rc;

	const struct CMUnitTest tcp_listen_tests[] = {
		cmocka_unit_test(test_listen_unbound_ipv4),
#ifdef HAVE_IPV6
		cmocka_unit_test(test_listen_unbound_ipv6),
#endif /* HAVE_IPV6 */
	};

	rc = cmocka_run_group_tests(tcp_listen_tests, setup, teardown);

	return rc;
}
