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

static int setup_echo_srv_udp_ipv4(void **state)
{
	torture_setup_echo_srv_udp_ipv4(state);

	return 0;
}

#ifdef HAVE_IPV6
static int setup_echo_srv_udp_ipv6(void **state)
{
	torture_setup_echo_srv_udp_ipv6(state);

	return 0;
}
#endif

static int teardown(void **state)
{
	torture_teardown_echo_srv(state);

	return 0;
}

static void test_sendmsg_recvmsg_ipv4(void **state)
{
	struct torture_address addr = {
		.sa_socklen = sizeof(struct sockaddr_in),
	};
	char send_buf[64] = {0};
	char recv_buf[64] = {0};
	ssize_t ret;
	int rc;
	int i;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	assert_int_not_equal(s, -1);

	addr.sa.in.sin_family = AF_INET;
	addr.sa.in.sin_port = htons(torture_server_port());

	rc = inet_pton(AF_INET,
		       torture_server_address(AF_INET),
		       &addr.sa.in.sin_addr);
	assert_int_equal(rc, 1);

	for (i = 0; i < 10; i++) {
		char ip[INET_ADDRSTRLEN] = {0};
		const char *a;
		struct torture_address srv_in = {
			.sa_socklen = sizeof(struct sockaddr_in),
		};
		struct msghdr s_msg;
		struct msghdr r_msg;
		struct iovec s_iov;
		struct iovec r_iov;

		snprintf(send_buf, sizeof(send_buf), "packet.%d", i);

		ZERO_STRUCT(s_msg);

		s_msg.msg_name = &addr.sa.s;
		s_msg.msg_namelen = addr.sa_socklen;

		s_iov.iov_base = send_buf;
		s_iov.iov_len = sizeof(send_buf);

		s_msg.msg_iov = &s_iov;
		s_msg.msg_iovlen = 1;

		ret = sendmsg(s, &s_msg, 0);
		assert_int_not_equal(ret, -1);

		ZERO_STRUCT(r_msg);

		r_msg.msg_name = &srv_in.sa.s;
		r_msg.msg_namelen = srv_in.sa_socklen;

		r_iov.iov_base = recv_buf;
		r_iov.iov_len = sizeof(recv_buf);

		r_msg.msg_iov = &r_iov;
		r_msg.msg_iovlen = 1;

		ret = recvmsg(s, &r_msg, 0);
		assert_int_not_equal(ret, -1);

		assert_int_equal(r_msg.msg_namelen, sizeof(struct sockaddr_in));

		a = inet_ntop(AF_INET, &srv_in.sa.in.sin_addr, ip, sizeof(ip));
		assert_non_null(a);
		assert_string_equal(a, torture_server_address(AF_INET));

		assert_memory_equal(send_buf, recv_buf, sizeof(send_buf));
	}

	close(s);
}

#ifdef HAVE_IPV6
static void test_sendmsg_recvmsg_ipv6(void **state)
{
	struct torture_address addr = {
		.sa_socklen = sizeof(struct sockaddr_in6),
	};
	char send_buf[64] = {0};
	char recv_buf[64] = {0};
	ssize_t ret;
	int rc;
	int i;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	assert_int_not_equal(s, -1);

	addr.sa.in6.sin6_family = AF_INET6;
	addr.sa.in6.sin6_port = htons(torture_server_port());

	rc = inet_pton(AF_INET6,
		       torture_server_address(AF_INET6),
		       &addr.sa.in6.sin6_addr);
	assert_int_equal(rc, 1);

	for (i = 0; i < 10; i++) {
		char ip[INET6_ADDRSTRLEN] = {0};
		const char *a;
		struct torture_address srv_in6 = {
			.sa_socklen = sizeof(struct sockaddr_in6),
		};
		struct msghdr s_msg;
		struct msghdr r_msg;
		struct iovec s_iov;
		struct iovec r_iov;

		snprintf(send_buf, sizeof(send_buf), "packet.%d", i);

		ZERO_STRUCT(s_msg);

		s_msg.msg_name = &addr.sa.s;
		s_msg.msg_namelen = addr.sa_socklen;

		s_iov.iov_base = send_buf;
		s_iov.iov_len = sizeof(send_buf);

		s_msg.msg_iov = &s_iov;
		s_msg.msg_iovlen = 1;

		ret = sendmsg(s, &s_msg, 0);
		assert_int_not_equal(ret, -1);

		ZERO_STRUCT(r_msg);
		r_msg.msg_name = &srv_in6.sa.s;
		r_msg.msg_namelen = srv_in6.sa_socklen;

		r_iov.iov_base = recv_buf;
		r_iov.iov_len = sizeof(recv_buf);

		r_msg.msg_iov = &r_iov;
		r_msg.msg_iovlen = 1;

		ret = recvmsg(s, &r_msg, 0);
		assert_int_not_equal(ret, -1);

		assert_int_equal(r_msg.msg_namelen, sizeof(struct sockaddr_in6));

		a = inet_ntop(AF_INET6, &srv_in6.sa.in6.sin6_addr, ip, sizeof(ip));
		assert_non_null(a);
		assert_string_equal(a, torture_server_address(AF_INET6));

		assert_memory_equal(send_buf, recv_buf, sizeof(send_buf));
	}

	close(s);
}
#endif

static void test_sendmsg_recvmsg_ipv4_connected(void **state)
{
	struct torture_address s_addr = {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};
	struct torture_address r_addr = {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};
	struct msghdr s_msg = {
		.msg_namelen = 0,
	};
	struct msghdr r_msg = {
		.msg_namelen = 0,
	};
	struct iovec iov;
	char ip[INET_ADDRSTRLEN] = {0};
	char payload[] = "PACKET";
	const char *a;
	ssize_t ret;
	int rc;
	int s;

	(void)state; /* unused */

	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	assert_int_not_equal(s, -1);

	s_addr.sa.in.sin_family = AF_INET;
	s_addr.sa.in.sin_port = htons(torture_server_port());

	rc = inet_pton(AF_INET,
		       torture_server_address(AF_INET),
		       &s_addr.sa.in.sin_addr);
	assert_int_equal(rc, 1);

	rc = connect(s, &s_addr.sa.s, s_addr.sa_socklen);

	iov.iov_base = (void *)payload;
	iov.iov_len = sizeof(payload);

	/* msg_name is NULL */

	s_msg.msg_iov = &iov;
	s_msg.msg_iovlen = 1;

	ret = sendmsg(s, &s_msg, 0);
	assert_int_not_equal(ret, -1);

	r_msg.msg_name = &r_addr.sa.ss;
	r_msg.msg_namelen = r_addr.sa_socklen;

	memset(payload, '0', sizeof(payload));

	r_msg.msg_iov = &iov;
	r_msg.msg_iovlen = 1;

	ret = recvmsg(s, &r_msg, 0);
	assert_int_not_equal(ret, -1);

	assert_int_equal(r_msg.msg_namelen, sizeof(struct sockaddr_in));

	a = inet_ntop(AF_INET, &r_addr.sa.in.sin_addr, ip, sizeof(ip));
	assert_non_null(a);
	assert_string_equal(a, torture_server_address(AF_INET));

	close(s);
}

static void test_sendmsg_recvmsg_ipv4_connected_null(void **state)
{
	struct torture_address s_addr = {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};
	struct msghdr s_msg = {
		.msg_namelen = 0,
	};
	struct msghdr r_msg = {
		.msg_namelen = 0,
	};
	struct iovec iov;
	char payload[] = "PACKET";
	ssize_t ret;
	int rc;
	int s;

	(void)state; /* unused */

	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	assert_int_not_equal(s, -1);

	s_addr.sa.in.sin_family = AF_INET;
	s_addr.sa.in.sin_port = htons(torture_server_port());

	rc = inet_pton(AF_INET,
		       torture_server_address(AF_INET),
		       &s_addr.sa.in.sin_addr);
	assert_int_equal(rc, 1);

	rc = connect(s, &s_addr.sa.s, s_addr.sa_socklen);

	/* msg_name = NULL */

	iov.iov_base = (void *)payload;
	iov.iov_len = sizeof(payload);

	s_msg.msg_iov = &iov;
	s_msg.msg_iovlen = 1;

	ret = sendmsg(s, &s_msg, 0);
	assert_int_not_equal(ret, -1);

	/* msg_name = NULL */

	memset(payload, '0', sizeof(payload));

	r_msg.msg_iov = &iov;
	r_msg.msg_iovlen = 1;

	ret = recvmsg(s, &r_msg, 0);
	assert_int_not_equal(ret, -1);

	assert_int_equal(r_msg.msg_namelen, 0);
	assert_null(r_msg.msg_name);

	close(s);
}

static void test_sendmsg_recvmsg_ipv4_connected_namelen(void **state)
{
	struct torture_address s_addr = {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};
	struct msghdr s_msg = {
		.msg_namelen = 0,
	};
	struct msghdr r_msg = {
		.msg_namelen = sizeof(struct sockaddr_storage),
	};
	struct iovec iov;
	char payload[] = "PACKET";
	ssize_t ret;
	int rc;
	int s;

	(void)state; /* unused */

	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	assert_int_not_equal(s, -1);

	s_addr.sa.in.sin_family = AF_INET;
	s_addr.sa.in.sin_port = htons(torture_server_port());

	rc = inet_pton(AF_INET,
		       torture_server_address(AF_INET),
		       &s_addr.sa.in.sin_addr);
	assert_int_equal(rc, 1);

	rc = connect(s, &s_addr.sa.s, s_addr.sa_socklen);

	/* msg_name = NULL */

	iov.iov_base = (void *)payload;
	iov.iov_len = sizeof(payload);

	s_msg.msg_iov = &iov;
	s_msg.msg_iovlen = 1;

	ret = sendmsg(s, &s_msg, 0);
	assert_int_not_equal(ret, -1);

	/* msg_name = NULL */

	memset(payload, '0', sizeof(payload));

	r_msg.msg_iov = &iov;
	r_msg.msg_iovlen = 1;

	ret = recvmsg(s, &r_msg, 0);
	assert_int_not_equal(ret, -1);

	close(s);
}

int main(void) {
	int rc;

	const struct CMUnitTest sendmsg_tests[] = {
		cmocka_unit_test_setup_teardown(test_sendmsg_recvmsg_ipv4,
						setup_echo_srv_udp_ipv4,
						teardown),
#ifdef HAVE_IPV6
		cmocka_unit_test_setup_teardown(test_sendmsg_recvmsg_ipv6,
						setup_echo_srv_udp_ipv6,
						teardown),
#endif
		cmocka_unit_test_setup_teardown(test_sendmsg_recvmsg_ipv4_connected,
						setup_echo_srv_udp_ipv4,
						teardown),
		cmocka_unit_test_setup_teardown(test_sendmsg_recvmsg_ipv4_connected_null,
						setup_echo_srv_udp_ipv4,
						teardown),
		cmocka_unit_test_setup_teardown(test_sendmsg_recvmsg_ipv4_connected_namelen,
						setup_echo_srv_udp_ipv4,
						teardown),
	};

	rc = cmocka_run_group_tests(sendmsg_tests, NULL, NULL);

	return rc;
}
