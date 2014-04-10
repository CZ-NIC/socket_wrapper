#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "config.h"
#include "torture.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

static void setup_echo_srv_tcp_ipv4(void **state)
{
	torture_setup_echo_srv_tcp_ipv4(state);
}

#ifdef HAVE_IPV6
static void setup_echo_srv_tcp_ipv6(void **state)
{
	torture_setup_echo_srv_tcp_ipv6(state);
}
#endif

static void teardown(void **state)
{
	torture_teardown_echo_srv(state);
}

static void test_writev_readv_ipv4(void **state)
{
	struct sockaddr_in sin;
	socklen_t slen = sizeof(struct sockaddr_in);
	ssize_t ret;
	int rc;
	int i;
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

	for (i = 1; i < 10; i++) {
		char send_buf[10][64];
		char recv_buf[10][64];
		struct iovec iov_send[10];
		struct iovec iov_recv[10];
		int j;

		for (j = 0; j < i; j++) {
			memset(send_buf[j], 0, 64);
			snprintf(send_buf[j], sizeof(send_buf[j]),
				 "packet.%d", j);

			iov_send[j].iov_base = send_buf[j];
			iov_send[j].iov_len = strlen(send_buf[j]);

			iov_recv[j].iov_base = recv_buf[j];
			iov_recv[j].iov_len = strlen(send_buf[j]);
		}

		ret = writev(s, iov_send, j);
		assert_int_not_equal(ret, -1);

		ret = readv(s, iov_recv, j);
		assert_int_not_equal(ret, -1);

		for (j = 0; j < i; j++) {
			assert_int_equal(iov_send[j].iov_len,
					 iov_recv[j].iov_len);

			assert_memory_equal(iov_send[j].iov_base,
					    iov_recv[j].iov_base,
					    iov_send[j].iov_len);
		}
	}

	close(s);
}

#ifdef HAVE_IPV6
static void test_writev_readv_ipv6(void **state)
{
	struct sockaddr_in6 sin6;
	socklen_t slen = sizeof(struct sockaddr_in6);
	ssize_t ret;
	int rc;
	int i;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	assert_int_not_equal(s, -1);

	ZERO_STRUCT(sin6);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(torture_server_port());

	rc = inet_pton(AF_INET6,
			torture_server_address(AF_INET6),
			&sin6.sin6_addr);
	assert_int_equal(rc, 1);

	rc = connect(s, (struct sockaddr *)&sin6, slen);
	assert_int_equal(rc, 0);

	for (i = 1; i < 10; i++) {
		char send_buf[10][64];
		char recv_buf[10][64];
		struct iovec iov_send[10];
		struct iovec iov_recv[10];
		int j;

		for (j = 0; j < i; j++) {
			memset(send_buf[j], 0, 64);
			snprintf(send_buf[j], sizeof(send_buf[j]),
				 "packet.%d", j);

			iov_send[j].iov_base = send_buf[j];
			iov_send[j].iov_len = strlen(send_buf[j]);

			iov_recv[j].iov_base = recv_buf[j];
			iov_recv[j].iov_len = strlen(send_buf[j]);
		}

		ret = writev(s, iov_send, j);
		assert_int_not_equal(ret, -1);

		ret = readv(s, iov_recv, j);
		assert_int_not_equal(ret, -1);

		for (j = 0; j < i; j++) {
			assert_int_equal(iov_send[j].iov_len,
					 iov_recv[j].iov_len);

			assert_memory_equal(iov_send[j].iov_base,
					    iov_recv[j].iov_base,
					    iov_send[j].iov_len);
		}
	}

	close(s);
}
#endif

int main(void) {
	int rc;

	const UnitTest tests[] = {
		unit_test_setup_teardown(test_writev_readv_ipv4, setup_echo_srv_tcp_ipv4, teardown),
#ifdef HAVE_IPV6
		unit_test_setup_teardown(test_writev_readv_ipv6, setup_echo_srv_tcp_ipv6, teardown),
#endif
	};

	rc = run_tests(tests);

	return rc;
}
