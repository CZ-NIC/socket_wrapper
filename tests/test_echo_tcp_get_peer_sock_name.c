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

static void setup_echo_srv_tcp_ipv4(void **state)
{
	torture_setup_echo_srv_tcp_ipv4(state);
	setenv("SOCKET_WRAPPER_DEFAULT_IFACE", "20", 1);
}

static void teardown(void **state)
{
	torture_teardown_echo_srv(state);
}

static void _assert_sockaddr_equal(struct sockaddr_storage *ss, const char *a,
				   const char * const file, const int line)
{
	char ip[INET6_ADDRSTRLEN] = { 0 };
	struct sockaddr_in *sinp = (struct sockaddr_in *)ss;
	const char *p;

	p = inet_ntop(ss->ss_family,
		      &sinp->sin_addr,
		      ip,
		      sizeof(ip));
	assert_non_null(p);

	_assert_string_equal(ip, a, file, line);
}

#define assert_sockaddr_equal(ss, a) \
	_assert_sockaddr_equal(ss, a, __FILE__, __LINE__)

static void _assert_sockaddr_port_equal(struct sockaddr_storage *ss, const char *a,
					uint16_t port,
					const char * const file, const int line)
{
	struct sockaddr_in *sinp = (struct sockaddr_in *)ss;

	_assert_sockaddr_equal(ss, a, file, line);

	_assert_int_equal(ntohs(sinp->sin_port), port, file, line);
}

#define assert_sockaddr_port_equal(ss, a, prt) \
	_assert_sockaddr_port_equal(ss, a, prt, __FILE__, __LINE__)

static void _assert_sockaddr_port_range_equal(struct sockaddr_storage *ss, const char *a,
					      uint16_t min_port, uint16_t max_port,
					      const char * const file, const int line)
{
	struct sockaddr_in *sinp = (struct sockaddr_in *)ss;

	_assert_sockaddr_equal(ss, a, file, line);

	_assert_in_range(ntohs(sinp->sin_port),
			 min_port,
			 max_port,
			 file,
			 line);
}

#define assert_sockaddr_port_range_equal(ss, a, min_prt, max_prt) \
	_assert_sockaddr_port_range_equal(ss, a, min_prt, max_prt, __FILE__, __LINE__)

static void test_connect_getsockname_getpeername(void **state)
{
	struct sockaddr_in sin;
	socklen_t slen = sizeof(struct sockaddr_in);
	struct sockaddr_storage cli_ss1;
	socklen_t cli_ss1_len;
	struct sockaddr_storage srv_ss1;
	socklen_t srv_ss1_len;
	int rc;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	/* Bind client address to wildcard address */
	ZERO_STRUCT(sin);
	sin.sin_family = AF_INET;

	rc = inet_pton(AF_INET, "127.0.0.20", &sin.sin_addr);
	assert_int_equal(rc, 1);

	rc = bind(s, (struct sockaddr *)&sin, slen);
	assert_return_code(rc, errno);

	ZERO_STRUCT(cli_ss1);
	cli_ss1_len = sizeof(cli_ss1);
	rc = getsockname(s, (struct sockaddr *)&cli_ss1, &cli_ss1_len);
	assert_return_code(rc, errno);
	assert_sockaddr_port_range_equal(&cli_ss1, "127.0.0.20", 1024, 65535);

	ZERO_STRUCT(srv_ss1);
	srv_ss1_len = sizeof(srv_ss1);
	rc = getpeername(s, (struct sockaddr *)&srv_ss1, &srv_ss1_len);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, ENOTCONN);

	/* connect */
	ZERO_STRUCT(sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(torture_server_port());
	rc = inet_pton(AF_INET, torture_server_address(AF_INET), &sin.sin_addr);
	assert_int_equal(rc, 1);

	/* Connect */
	rc = connect(s, (struct sockaddr *)&sin, slen);
	assert_return_code(rc, errno);

	ZERO_STRUCT(cli_ss1);
	cli_ss1_len = sizeof(cli_ss1);
	rc = getsockname(s, (struct sockaddr *)&cli_ss1, &cli_ss1_len);
	assert_return_code(rc, errno);
	assert_sockaddr_port_range_equal(&cli_ss1, "127.0.0.20", 1024, 65535);

	ZERO_STRUCT(srv_ss1);
	srv_ss1_len = sizeof(srv_ss1);
	rc = getpeername(s, (struct sockaddr *)&srv_ss1, &srv_ss1_len);
	assert_return_code(rc, errno);
	assert_sockaddr_port_equal(&srv_ss1, "127.0.0.10", 7);

	close(s);
}

static void test_connect_getsockname_getpeername_port(void **state)
{
	struct sockaddr_in sin;
	socklen_t slen = sizeof(struct sockaddr_in);
	struct sockaddr_storage cli_ss1;
	socklen_t cli_ss1_len;
	struct sockaddr_storage srv_ss1;
	socklen_t srv_ss1_len;
	int rc;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	/* Bind client address to wildcard address */
	ZERO_STRUCT(sin);
	sin.sin_family = AF_INET;

	rc = inet_pton(AF_INET, "127.0.0.20", &sin.sin_addr);
	assert_int_equal(rc, 1);
	sin.sin_port = htons(12345);

	rc = bind(s, (struct sockaddr *)&sin, slen);
	assert_return_code(rc, errno);

	ZERO_STRUCT(cli_ss1);
	cli_ss1_len = sizeof(cli_ss1);
	rc = getsockname(s, (struct sockaddr *)&cli_ss1, &cli_ss1_len);
	assert_return_code(rc, errno);
	assert_sockaddr_port_equal(&cli_ss1, "127.0.0.20", 12345);

	ZERO_STRUCT(srv_ss1);
	srv_ss1_len = sizeof(srv_ss1);
	rc = getpeername(s, (struct sockaddr *)&srv_ss1, &srv_ss1_len);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, ENOTCONN);

	/* connect */
	ZERO_STRUCT(sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(torture_server_port());
	rc = inet_pton(AF_INET, torture_server_address(AF_INET), &sin.sin_addr);
	assert_int_equal(rc, 1);

	/* Connect */
	rc = connect(s, (struct sockaddr *)&sin, slen);
	assert_return_code(rc, errno);

	ZERO_STRUCT(cli_ss1);
	cli_ss1_len = sizeof(cli_ss1);
	rc = getsockname(s, (struct sockaddr *)&cli_ss1, &cli_ss1_len);
	assert_return_code(rc, errno);
	assert_sockaddr_port_equal(&cli_ss1, "127.0.0.20", 12345);

	ZERO_STRUCT(srv_ss1);
	srv_ss1_len = sizeof(srv_ss1);
	rc = getpeername(s, (struct sockaddr *)&srv_ss1, &srv_ss1_len);
	assert_return_code(rc, errno);
	assert_sockaddr_port_equal(&srv_ss1, "127.0.0.10", 7);

	close(s);
}

static void test_connect_getsockname_getpeername_any(void **state)
{
	struct sockaddr_in sin;
	socklen_t slen = sizeof(struct sockaddr_in);
	struct sockaddr_storage cli_ss1;
	socklen_t cli_ss1_len;
	struct sockaddr_storage srv_ss1;
	socklen_t srv_ss1_len;
	int rc;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	/* Bind client address to wildcard address */
	ZERO_STRUCT(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);

	rc = bind(s, (struct sockaddr *)&sin, slen);
	assert_return_code(rc, errno);

	ZERO_STRUCT(cli_ss1);
	cli_ss1_len = sizeof(cli_ss1);
	rc = getsockname(s, (struct sockaddr *)&cli_ss1, &cli_ss1_len);
	assert_return_code(rc, errno);
	assert_sockaddr_port_range_equal(&cli_ss1, "0.0.0.0", 1024, 65535);

	ZERO_STRUCT(srv_ss1);
	srv_ss1_len = sizeof(srv_ss1);
	rc = getpeername(s, (struct sockaddr *)&srv_ss1, &srv_ss1_len);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, ENOTCONN);

	/* connect */
	ZERO_STRUCT(sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(torture_server_port());
	rc = inet_pton(AF_INET, torture_server_address(AF_INET), &sin.sin_addr);
	assert_int_equal(rc, 1);

	/* Connect */
	rc = connect(s, (struct sockaddr *)&sin, slen);
	assert_return_code(rc, errno);

	ZERO_STRUCT(cli_ss1);
	cli_ss1_len = sizeof(cli_ss1);
	rc = getsockname(s, (struct sockaddr *)&cli_ss1, &cli_ss1_len);
	assert_return_code(rc, errno);
	assert_sockaddr_port_range_equal(&cli_ss1, "127.0.0.20", 1024, 65535);

	ZERO_STRUCT(srv_ss1);
	srv_ss1_len = sizeof(srv_ss1);
	rc = getpeername(s, (struct sockaddr *)&srv_ss1, &srv_ss1_len);
	assert_return_code(rc, errno);
	assert_sockaddr_port_equal(&srv_ss1, "127.0.0.10", 7);

	close(s);
}

static void test_connect_getsockname_getpeername_any_port(void **state)
{
	struct sockaddr_in sin;
	socklen_t slen = sizeof(struct sockaddr_in);
	struct sockaddr_storage cli_ss1;
	socklen_t cli_ss1_len;
	struct sockaddr_storage srv_ss1;
	socklen_t srv_ss1_len;
	int rc;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	/* Bind client address to wildcard address */
	ZERO_STRUCT(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(12345);

	rc = bind(s, (struct sockaddr *)&sin, slen);
	assert_return_code(rc, errno);

	ZERO_STRUCT(cli_ss1);
	cli_ss1_len = sizeof(cli_ss1);
	rc = getsockname(s, (struct sockaddr *)&cli_ss1, &cli_ss1_len);
	assert_return_code(rc, errno);
	assert_sockaddr_port_equal(&cli_ss1, "0.0.0.0", 12345);

	ZERO_STRUCT(srv_ss1);
	srv_ss1_len = sizeof(srv_ss1);
	rc = getpeername(s, (struct sockaddr *)&srv_ss1, &srv_ss1_len);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, ENOTCONN);

	/* connect */
	ZERO_STRUCT(sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(torture_server_port());
	rc = inet_pton(AF_INET, torture_server_address(AF_INET), &sin.sin_addr);
	assert_int_equal(rc, 1);

	/* Connect */
	rc = connect(s, (struct sockaddr *)&sin, slen);
	assert_return_code(rc, errno);

	ZERO_STRUCT(cli_ss1);
	cli_ss1_len = sizeof(cli_ss1);
	rc = getsockname(s, (struct sockaddr *)&cli_ss1, &cli_ss1_len);
	assert_return_code(rc, errno);
	assert_sockaddr_port_equal(&cli_ss1, "127.0.0.20", 12345);

	ZERO_STRUCT(srv_ss1);
	srv_ss1_len = sizeof(srv_ss1);
	rc = getpeername(s, (struct sockaddr *)&srv_ss1, &srv_ss1_len);
	assert_return_code(rc, errno);
	assert_sockaddr_port_equal(&srv_ss1, "127.0.0.10", 7);

	close(s);
}

int main(void) {
	int rc;

	const UnitTest tests[] = {
		unit_test_setup_teardown(test_connect_getsockname_getpeername,
					 setup_echo_srv_tcp_ipv4,
					 teardown),
		unit_test_setup_teardown(test_connect_getsockname_getpeername_port,
					 setup_echo_srv_tcp_ipv4,
					 teardown),
		unit_test_setup_teardown(test_connect_getsockname_getpeername_any,
					 setup_echo_srv_tcp_ipv4,
					 teardown),
		unit_test_setup_teardown(test_connect_getsockname_getpeername_any_port,
					 setup_echo_srv_tcp_ipv4,
					 teardown),
	};

	rc = run_tests(tests);

	return rc;
}
