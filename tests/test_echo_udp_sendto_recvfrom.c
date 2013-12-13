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

static void setup_echo_srv_udp_ipv6(void **state)
{
	torture_setup_echo_srv_udp_ipv6(state);
}

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
	sin.sin_port = htons(TORTURE_ECHO_SRV_PORT);

	rc = inet_pton(sin.sin_family, TORTURE_ECHO_SRV_IPV4, &sin.sin_addr);
	assert_int_equal(rc, 1);

	for (i = 0; i < 10; i++) {
		char ip[INET_ADDRSTRLEN] = {0};
		const char *a;
		struct sockaddr_in srv_in;
		socklen_t rlen = sizeof(srv_in);

		snprintf(send_buf, sizeof(send_buf), "packet.%d", i);

		ret = sendto(s,
			     send_buf,
			     sizeof(send_buf),
			     0,
			     (struct sockaddr *)(void *)&sin,
			     slen);
		assert_int_not_equal(ret, -1);

		ret = recvfrom(s,
			       recv_buf,
			       sizeof(recv_buf),
			       0,
			       (struct sockaddr *)&srv_in,
			       &rlen);
		assert_int_not_equal(ret, -1);

		a = inet_ntop(AF_INET, &srv_in.sin_addr, ip, sizeof(ip));
		assert_non_null(a);
		assert_string_equal(a, TORTURE_ECHO_SRV_IPV4);

		assert_memory_equal(send_buf, recv_buf, sizeof(send_buf));
	}

	ret = sendto(s,
		     send_buf,
		     sizeof(send_buf),
		     0,
		     (struct sockaddr *)(void *)&sin,
		     slen);
	assert_int_not_equal(ret, -1);

	ret = recvfrom(s,
		       recv_buf,
		       sizeof(recv_buf),
		       0,
		       NULL,
		       NULL);
	assert_int_not_equal(ret, -1);

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
	sin6.sin6_port = htons(TORTURE_ECHO_SRV_PORT);

	rc = inet_pton(AF_INET6, TORTURE_ECHO_SRV_IPV6, &sin6.sin6_addr);
	assert_int_equal(rc, 1);

	for (i = 0; i < 10; i++) {
		char ip[INET6_ADDRSTRLEN] = {0};
		const char *a;
		struct sockaddr_in6 srv_in6;
		socklen_t rlen = sizeof(srv_in6);

		snprintf(send_buf, sizeof(send_buf), "packet.%d", i);

		ret = sendto(s,
			     send_buf,
			     sizeof(send_buf),
			     0,
			     (struct sockaddr *)(void *)&sin6,
			     slen);
		assert_int_not_equal(ret, -1);

		ret = recvfrom(s,
			       recv_buf,
			       sizeof(recv_buf),
			       0,
			       (struct sockaddr *)&srv_in6,
			       &rlen);
		assert_int_not_equal(ret, -1);

		a = inet_ntop(AF_INET6, &srv_in6.sin6_addr, ip, sizeof(ip));
		assert_non_null(a);
		assert_string_equal(a, TORTURE_ECHO_SRV_IPV6);

		assert_memory_equal(send_buf, recv_buf, sizeof(send_buf));
	}

	ret = sendto(s,
		     send_buf,
		     sizeof(send_buf),
		     0,
		     (struct sockaddr *)(void *)&sin6,
		     slen);
	assert_int_not_equal(ret, -1);

	ret = recvfrom(s,
		       recv_buf,
		       sizeof(recv_buf),
		       0,
		       NULL,
		       NULL);
	assert_int_not_equal(ret, -1);

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
