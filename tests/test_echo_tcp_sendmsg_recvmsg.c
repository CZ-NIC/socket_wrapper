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

static int setup_echo_srv_tcp_ipv4(void **state)
{
	torture_setup_echo_srv_tcp_ipv4(state);

	return 0;
}

#ifdef HAVE_IPV6
static int setup_echo_srv_tcp_ipv6(void **state)
{
	torture_setup_echo_srv_tcp_ipv6(state);

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
		.sa_socklen = sizeof(struct sockaddr_storage),
	};
	char send_buf[64] = {0};
	char recv_buf[64] = {0};
	ssize_t ret;
	int rc;
	int i;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_int_not_equal(s, -1);

	addr.sa.in.sin_family = AF_INET;
	addr.sa.in.sin_port = htons(torture_server_port());

	rc = inet_pton(AF_INET,
		       torture_server_address(AF_INET),
		       &addr.sa.in.sin_addr);
	assert_int_equal(rc, 1);

	rc = connect(s, &addr.sa.s, addr.sa_socklen);

	for (i = 0; i < 10; i++) {
		struct torture_address reply_addr = {
			.sa_socklen = sizeof(struct sockaddr_storage),
		};
		struct msghdr s_msg = {
			.msg_namelen = 0,
		};
		struct msghdr r_msg = {
			.msg_namelen = 0,
		};
		struct iovec s_iov;
		struct iovec r_iov;

		snprintf(send_buf, sizeof(send_buf), "packet.%d", i);

		/* This should be ignored */
		rc = inet_pton(AF_INET,
			       "127.0.0.1",
			       &addr.sa.in.sin_addr);
		assert_int_equal(rc, 1);

		s_msg.msg_name = &addr.sa.s;
		s_msg.msg_namelen = addr.sa_socklen;

		s_iov.iov_base = send_buf;
		s_iov.iov_len = sizeof(send_buf);

		s_msg.msg_iov = &s_iov;
		s_msg.msg_iovlen = 1;

		ret = sendmsg(s, &s_msg, 0);
		assert_int_not_equal(ret, -1);

		r_msg.msg_name = &reply_addr.sa.s;
		r_msg.msg_namelen = reply_addr.sa_socklen;

		r_iov.iov_base = recv_buf;
		r_iov.iov_len = sizeof(recv_buf);

		r_msg.msg_iov = &r_iov;
		r_msg.msg_iovlen = 1;

		ret = recvmsg(s, &r_msg, 0);
		assert_int_not_equal(ret, -1);

		assert_int_equal(r_msg.msg_namelen, 0);

		assert_memory_equal(send_buf, recv_buf, sizeof(send_buf));
	}

	close(s);
}

#ifdef HAVE_IPV6
static void test_sendmsg_recvmsg_ipv6(void **state)
{
	struct torture_address addr = {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};
	char send_buf[64] = {0};
	char recv_buf[64] = {0};
	ssize_t ret;
	int rc;
	int i;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	assert_int_not_equal(s, -1);

	addr.sa.in.sin_family = AF_INET6;
	addr.sa.in.sin_port = htons(torture_server_port());

	rc = inet_pton(AF_INET6,
		       torture_server_address(AF_INET6),
		       &addr.sa.in6.sin6_addr);
	assert_int_equal(rc, 1);

	rc = connect(s, &addr.sa.s, addr.sa_socklen);

	for (i = 0; i < 10; i++) {
		struct torture_address reply_addr = {
			.sa_socklen = sizeof(struct sockaddr_in),
		};
		struct msghdr s_msg = {
			.msg_namelen = 0,
		};
		struct msghdr r_msg = {
			.msg_namelen = 0,
		};
		struct iovec s_iov;
		struct iovec r_iov;

		snprintf(send_buf, sizeof(send_buf), "packet.%d", i);

		s_iov.iov_base = send_buf;
		s_iov.iov_len = sizeof(send_buf);

		s_msg.msg_iov = &s_iov;
		s_msg.msg_iovlen = 1;

		ret = sendmsg(s, &s_msg, 0);
		assert_int_not_equal(ret, -1);

		r_msg.msg_name = &reply_addr.sa.s;
		r_msg.msg_namelen = reply_addr.sa_socklen;

		r_iov.iov_base = recv_buf;
		r_iov.iov_len = sizeof(recv_buf);

		r_msg.msg_iov = &r_iov;
		r_msg.msg_iovlen = 1;

		ret = recvmsg(s, &r_msg, 0);
		assert_int_not_equal(ret, -1);

		assert_int_equal(r_msg.msg_namelen, 0);

		assert_memory_equal(send_buf, recv_buf, sizeof(send_buf));
	}

	close(s);
}
#endif

static void test_sendmsg_recvmsg_ipv4_null(void **state)
{
	struct torture_address send_addr = {
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

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_int_not_equal(s, -1);

	send_addr.sa.in.sin_family = AF_INET;
	send_addr.sa.in.sin_port = htons(torture_server_port());

	rc = inet_pton(AF_INET,
		       torture_server_address(AF_INET),
		       &send_addr.sa.in.sin_addr);
	assert_int_equal(rc, 1);

	rc = connect(s, &send_addr.sa.s, send_addr.sa_socklen);

	/* msg_name = NULL */

	iov.iov_base = (void *)payload;
	iov.iov_len = sizeof(payload);

	s_msg.msg_iov = &iov;
	s_msg.msg_iovlen = 1;

	ret = sendmsg(s, &s_msg, 0);
	assert_int_not_equal(ret, -1);

	/* msg_name = NULL */

	memset(payload, 0, sizeof(payload));

	r_msg.msg_iov = &iov;
	r_msg.msg_iovlen = 1;

	ret = recvmsg(s, &r_msg, 0);
	assert_int_not_equal(ret, -1);

	assert_int_equal(r_msg.msg_namelen, 0);
	assert_null(r_msg.msg_name);

	close(s);
}

int main(void) {
	int rc;

	const struct CMUnitTest sendmsg_tests[] = {
		cmocka_unit_test_setup_teardown(test_sendmsg_recvmsg_ipv4,
						setup_echo_srv_tcp_ipv4,
						teardown),
		cmocka_unit_test_setup_teardown(test_sendmsg_recvmsg_ipv4_null,
						setup_echo_srv_tcp_ipv4,
						teardown),
#ifdef HAVE_IPV6
		cmocka_unit_test_setup_teardown(test_sendmsg_recvmsg_ipv6,
						setup_echo_srv_tcp_ipv6,
						teardown),
#endif
	};

	rc = cmocka_run_group_tests(sendmsg_tests, NULL, NULL);

	return rc;
}
