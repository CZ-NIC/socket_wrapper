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

static void setup_echo_srv_udp_ipv4(void **state)
{
	torture_setup_echo_srv_udp_ipv4(state);
}

#ifdef HAVE_IPV6
static void setup_echo_srv_udp_ipv6(void **state)
{
	torture_setup_echo_srv_udp_ipv6(state);
}
#endif

static void teardown(void **state)
{
	torture_teardown_echo_srv(state);
}

static void test_sendto_recvfrom_ipv4(void **state)
{
	struct sockaddr_in sin;
	socklen_t slen = sizeof(struct sockaddr_in);
	char send_buf[64] = {0};
	char recv_buf[64] = {0};
	ssize_t ret;
	int rc;
	int i;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	assert_int_not_equal(s, -1);

	ZERO_STRUCT(sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(torture_server_port());

	rc = inet_pton(AF_INET,
		       torture_server_address(AF_INET),
		       &sin.sin_addr);
	assert_int_equal(rc, 1);

	for (i = 0; i < 10; i++) {
		char ip[INET_ADDRSTRLEN] = {0};
		const char *a;
		struct sockaddr_in srv_in;
		socklen_t rlen = sizeof(srv_in);
		struct msghdr s_msg;
		struct msghdr r_msg;
		struct iovec s_iov;
		struct iovec r_iov;

		snprintf(send_buf, sizeof(send_buf), "packet.%d", i);

		ZERO_STRUCT(s_msg);

		s_msg.msg_name = (struct sockaddr *)(void *)&sin;
		s_msg.msg_namelen = slen;

		s_iov.iov_base = send_buf;
		s_iov.iov_len = sizeof(send_buf);

		s_msg.msg_iov = &s_iov;
		s_msg.msg_iovlen = 1;

		ret = sendmsg(s, &s_msg, 0);
		assert_int_not_equal(ret, -1);

		ZERO_STRUCT(r_msg);

		r_msg.msg_name = (struct sockaddr *)(void *)&srv_in;
		r_msg.msg_namelen = rlen;

		r_iov.iov_base = recv_buf;
		r_iov.iov_len = sizeof(recv_buf);

		r_msg.msg_iov = &r_iov;
		r_msg.msg_iovlen = 1;

		ret = recvmsg(s, &r_msg, 0);
		assert_int_not_equal(ret, -1);

		a = inet_ntop(AF_INET, &srv_in.sin_addr, ip, sizeof(ip));
		assert_non_null(a);
		assert_string_equal(a, torture_server_address(AF_INET));

		assert_memory_equal(send_buf, recv_buf, sizeof(send_buf));
	}

	close(s);
}

#ifdef HAVE_IPV6
static void test_sendto_recvfrom_ipv6(void **state)
{
	struct sockaddr_in6 sin6;
	socklen_t slen = sizeof(struct sockaddr_in6);
	char send_buf[64] = {0};
	char recv_buf[64] = {0};
	ssize_t ret;
	int rc;
	int i;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	assert_int_not_equal(s, -1);

	ZERO_STRUCT(sin6);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(torture_server_port());

	rc = inet_pton(AF_INET6,
		       torture_server_address(AF_INET6),
		       &sin6.sin6_addr);
	assert_int_equal(rc, 1);

	for (i = 0; i < 10; i++) {
		char ip[INET6_ADDRSTRLEN] = {0};
		const char *a;
		struct sockaddr_in6 srv_in6;
		socklen_t rlen = sizeof(srv_in6);
		struct msghdr s_msg;
		struct msghdr r_msg;
		struct iovec s_iov;
		struct iovec r_iov;

		snprintf(send_buf, sizeof(send_buf), "packet.%d", i);

		ZERO_STRUCT(s_msg);

		s_msg.msg_name = (struct sockaddr *)(void *)&sin6;
		s_msg.msg_namelen = slen;

		s_iov.iov_base = send_buf;
		s_iov.iov_len = sizeof(send_buf);

		s_msg.msg_iov = &s_iov;
		s_msg.msg_iovlen = 1;

		ret = sendmsg(s, &s_msg, 0);
		assert_int_not_equal(ret, -1);

		ZERO_STRUCT(r_msg);
		r_msg.msg_name = (struct sockaddr *)(void *)&srv_in6;
		r_msg.msg_namelen = rlen;

		r_iov.iov_base = recv_buf;
		r_iov.iov_len = sizeof(recv_buf);

		r_msg.msg_iov = &r_iov;
		r_msg.msg_iovlen = 1;

		ret = recvmsg(s, &r_msg, 0);
		assert_int_not_equal(ret, -1);

		a = inet_ntop(AF_INET6, &srv_in6.sin6_addr, ip, sizeof(ip));
		assert_non_null(a);
		assert_string_equal(a, torture_server_address(AF_INET6));

		assert_memory_equal(send_buf, recv_buf, sizeof(send_buf));
	}

	close(s);
}
#endif

int main(void) {
	int rc;

	const UnitTest tests[] = {
		unit_test_setup_teardown(test_sendto_recvfrom_ipv4, setup_echo_srv_udp_ipv4, teardown),
#ifdef HAVE_IPV6
		unit_test_setup_teardown(test_sendto_recvfrom_ipv6, setup_echo_srv_udp_ipv6, teardown),
#endif
	};

	rc = run_tests(tests);

	return rc;
}
