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

static void _assert_sockaddr_equal(struct torture_address *addr, const char *a,
				   const char * const file, const int line)
{
	char ip[INET6_ADDRSTRLEN] = { 0 };
	const char *p;

	p = inet_ntop(addr->sa.ss.ss_family,
		      addr->sa.ss.ss_family == AF_INET6 ?
		          (void *)&addr->sa.in6.sin6_addr :
		          (void *)&addr->sa.in.sin_addr,
		      ip,
		      sizeof(ip));
	_assert_true(cast_ptr_to_largest_integral_type(p),
		     "inet_ntop: Failed to convert IP address", file, line);

	_assert_string_equal(ip, a, file, line);
}

#define assert_sockaddr_equal(ss, a) \
	_assert_sockaddr_equal(ss, a, __FILE__, __LINE__)

static void _assert_sockaddr_port_equal(struct torture_address *addr,
					const char *a,
					uint16_t port,
					const char * const file, const int line)
{
	uint16_t n_port;

	_assert_sockaddr_equal(addr, a, file, line);

	switch(addr->sa.ss.ss_family) {
	case AF_INET:
		n_port = addr->sa.in.sin_port;
	case AF_INET6:
		n_port = addr->sa.in6.sin6_port;
	default:
		return;
	}

	_assert_int_equal(ntohs(n_port), port, file, line);
}

#define assert_sockaddr_port_equal(ss, a, prt) \
	_assert_sockaddr_port_equal(ss, a, prt, __FILE__, __LINE__)

static void _assert_sockaddr_port_range_equal(struct torture_address *addr,
					      const char *a,
					      uint16_t min_port, uint16_t max_port,
					      const char * const file, const int line)
{
	uint16_t n_port;

	_assert_sockaddr_equal(addr, a, file, line);

	switch(addr->sa.ss.ss_family) {
	case AF_INET:
		n_port = addr->sa.in.sin_port;
	case AF_INET6:
		n_port = addr->sa.in6.sin6_port;
	default:
		return;
	}

	_assert_in_range(ntohs(n_port),
			 min_port,
			 max_port,
			 file,
			 line);
}

#define assert_sockaddr_port_range_equal(ss, a, min_prt, max_prt) \
	_assert_sockaddr_port_range_equal(ss, a, min_prt, max_prt, __FILE__, __LINE__)

static void test_connect_getsockname_getpeername(void **state)
{
	struct torture_address addr = {
		.sa_socklen = sizeof(struct sockaddr_in),
	};
	struct torture_address cli_addr = {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};
	struct torture_address srv_addr = {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};
	int rc;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	/* Bind client address to wildcard address */
	addr.sa.in.sin_family = AF_INET;

	rc = inet_pton(AF_INET, "127.0.0.20", &addr.sa.in.sin_addr);
	assert_int_equal(rc, 1);

	rc = bind(s, &addr.sa.s, addr.sa_socklen);
	assert_return_code(rc, errno);

	rc = getsockname(s, &cli_addr.sa.s, &cli_addr.sa_socklen);
	assert_return_code(rc, errno);
	assert_sockaddr_port_range_equal(&cli_addr, "127.0.0.20", 1024, 65535);

	rc = getpeername(s, &addr.sa.s, &addr.sa_socklen);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, ENOTCONN);

	/* connect */
	addr = (struct torture_address) {
		.sa_socklen = sizeof(struct sockaddr_in),
		.sa.in = (struct sockaddr_in) {
			.sin_family = AF_INET,
			.sin_port = htons(torture_server_port()),
		},
	};
	rc = inet_pton(AF_INET,
		       torture_server_address(AF_INET),
		       &addr.sa.in.sin_addr);
	assert_int_equal(rc, 1);

	/* Connect */
	rc = connect(s, &addr.sa.s, addr.sa_socklen);
	assert_return_code(rc, errno);

	cli_addr = (struct torture_address) {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};

	rc = getsockname(s, &cli_addr.sa.s, &cli_addr.sa_socklen);
	assert_return_code(rc, errno);
	assert_sockaddr_port_range_equal(&cli_addr, "127.0.0.20", 1024, 65535);

	srv_addr = (struct torture_address) {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};

	rc = getpeername(s, &srv_addr.sa.s, &srv_addr.sa_socklen);
	assert_return_code(rc, errno);
	assert_sockaddr_port_equal(&srv_addr, "127.0.0.10", 7);

	close(s);
}

static void test_connect_getsockname_getpeername_port(void **state)
{
	struct torture_address addr = {
		.sa_socklen = sizeof(struct sockaddr_in),
	};
	struct torture_address cli_addr = {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};
	struct torture_address srv_addr = {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};
	int rc;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	/* Bind client address to wildcard address */
	addr.sa.in.sin_family = AF_INET;

	rc = inet_pton(AF_INET, "127.0.0.20", &addr.sa.in.sin_addr);
	assert_int_equal(rc, 1);
	addr.sa.in.sin_port = htons(12345);

	rc = bind(s, &addr.sa.s, addr.sa_socklen);
	assert_return_code(rc, errno);

	rc = getsockname(s, &cli_addr.sa.s, &cli_addr.sa_socklen);
	assert_return_code(rc, errno);
	assert_sockaddr_port_equal(&cli_addr, "127.0.0.20", 12345);

	rc = getpeername(s, &srv_addr.sa.s, &srv_addr.sa_socklen);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, ENOTCONN);

	/* connect */
	addr = (struct torture_address) {
		.sa_socklen = sizeof(struct sockaddr_in),
		.sa.in = (struct sockaddr_in) {
			.sin_family = AF_INET,
			.sin_port = htons(torture_server_port()),
		},
	};

	rc = inet_pton(AF_INET,
		       torture_server_address(AF_INET),
		       &addr.sa.in.sin_addr);
	assert_int_equal(rc, 1);

	/* Connect */
	rc = connect(s, &addr.sa.s, addr.sa_socklen);
	assert_return_code(rc, errno);

	cli_addr = (struct torture_address) {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};

	rc = getsockname(s, &cli_addr.sa.s, &cli_addr.sa_socklen);
	assert_return_code(rc, errno);
	assert_sockaddr_port_equal(&cli_addr, "127.0.0.20", 12345);

	srv_addr = (struct torture_address) {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};

	rc = getpeername(s, &srv_addr.sa.s, &srv_addr.sa_socklen);
	assert_return_code(rc, errno);
	assert_sockaddr_port_equal(&srv_addr, "127.0.0.10", 7);

	close(s);
}

static void test_connect_getsockname_getpeername_any(void **state)
{
	struct torture_address addr = {
		.sa_socklen = sizeof(struct sockaddr_in),
	};
	struct torture_address cli_addr = {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};
	struct torture_address srv_addr = {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};
	int rc;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	/* Bind client address to wildcard address */
	addr.sa.in.sin_family = AF_INET;
	addr.sa.in.sin_addr.s_addr = htonl(INADDR_ANY);

	rc = bind(s, &addr.sa.s, addr.sa_socklen);
	assert_return_code(rc, errno);

	rc = getsockname(s, &cli_addr.sa.s, &cli_addr.sa_socklen);
	assert_return_code(rc, errno);
	assert_sockaddr_port_range_equal(&cli_addr, "0.0.0.0", 1024, 65535);

	rc = getpeername(s, &srv_addr.sa.s, &srv_addr.sa_socklen);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, ENOTCONN);

	/* connect */
	addr.sa.in.sin_family = AF_INET;
	addr.sa.in.sin_port = htons(torture_server_port());
	rc = inet_pton(AF_INET,
		       torture_server_address(AF_INET),
		       &addr.sa.in.sin_addr);
	assert_int_equal(rc, 1);

	/* Connect */
	rc = connect(s, &addr.sa.s, addr.sa_socklen);
	assert_return_code(rc, errno);

	cli_addr = (struct torture_address) {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};

	rc = getsockname(s, &cli_addr.sa.s, &cli_addr.sa_socklen);
	assert_return_code(rc, errno);
	assert_sockaddr_port_range_equal(&cli_addr, "127.0.0.20", 1024, 65535);

	srv_addr = (struct torture_address) {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};

	rc = getpeername(s, &srv_addr.sa.s, &srv_addr.sa_socklen);
	assert_return_code(rc, errno);
	assert_sockaddr_port_equal(&srv_addr, "127.0.0.10", 7);

	close(s);
}

static void test_connect_getsockname_getpeername_any_port(void **state)
{
	struct torture_address addr = {
		.sa_socklen = sizeof(struct sockaddr_in),
	};
	struct torture_address cli_addr = {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};
	struct torture_address srv_addr = {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};
	int rc;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	/* Bind client address to wildcard address */
	addr.sa.in.sin_family = AF_INET;
	addr.sa.in.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sa.in.sin_port = htons(12345);

	rc = bind(s, &addr.sa.s, addr.sa_socklen);
	assert_return_code(rc, errno);

	rc = getsockname(s, &cli_addr.sa.s, &cli_addr.sa_socklen);
	assert_return_code(rc, errno);
	assert_sockaddr_port_equal(&cli_addr, "0.0.0.0", 12345);

	rc = getpeername(s, &srv_addr.sa.s, &srv_addr.sa_socklen);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, ENOTCONN);

	/* connect */
	addr.sa.in.sin_family = AF_INET;
	addr.sa.in.sin_port = htons(torture_server_port());
	rc = inet_pton(AF_INET,
		       torture_server_address(AF_INET),
		       &addr.sa.in.sin_addr);
	assert_int_equal(rc, 1);

	/* Connect */
	rc = connect(s, &addr.sa.s, addr.sa_socklen);
	assert_return_code(rc, errno);

	cli_addr = (struct torture_address) {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};

	rc = getsockname(s, &cli_addr.sa.s, &cli_addr.sa_socklen);
	assert_return_code(rc, errno);
	assert_sockaddr_port_equal(&cli_addr, "127.0.0.20", 12345);

	srv_addr = (struct torture_address) {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};

	rc = getpeername(s, &srv_addr.sa.s, &srv_addr.sa_socklen);
	assert_return_code(rc, errno);
	assert_sockaddr_port_equal(&srv_addr, "127.0.0.10", 7);

	close(s);
}

static void test_connect_getsockname_getpeername_len(void **state)
{
	struct torture_address addr = {
		.sa_socklen = sizeof(struct sockaddr_in),
	};
	struct torture_address cli_addr = {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};
	struct torture_address srv_addr = {
		.sa_socklen = sizeof(struct sockaddr_storage),
	};
	socklen_t tmp_len;
	int rc;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_return_code(s, errno);

	/* connect */
	addr.sa.in.sin_family = AF_INET;
	addr.sa.in.sin_port = htons(torture_server_port());
	rc = inet_pton(AF_INET,
		       torture_server_address(AF_INET),
		       &addr.sa.in.sin_addr);
	assert_int_equal(rc, 1);

	/* Connect */
	rc = connect(s, &addr.sa.in, addr.sa_socklen);
	assert_return_code(rc, errno);

	/* Check with len=0 */
	cli_addr.sa_socklen = 0;
	rc = getsockname(s, &cli_addr.sa.s, &cli_addr.sa_socklen);
	assert_return_code(rc, errno);

	srv_addr.sa_socklen = 0;
	rc = getpeername(s, &srv_addr.sa.s, &srv_addr.sa_socklen);
	assert_return_code(rc, errno);

	/* Check with len=too small */
	cli_addr = (struct torture_address) {
		.sa_socklen = sizeof(struct sockaddr_in) - 2,
	};

	tmp_len = cli_addr.sa_socklen;
	rc = getsockname(s, &cli_addr.sa.s, &cli_addr.sa_socklen);
	assert_return_code(rc, errno);
	assert_int_equal(tmp_len + 2, cli_addr.sa_socklen);

	srv_addr = (struct torture_address) {
		.sa_socklen = sizeof(struct sockaddr_in) - 2,
	};

	tmp_len = srv_addr.sa_socklen;
	rc = getpeername(s, &srv_addr.sa.s, &srv_addr.sa_socklen);
	assert_return_code(rc, errno);
	assert_int_equal(tmp_len + 2, srv_addr.sa_socklen);

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
		unit_test_setup_teardown(test_connect_getsockname_getpeername_len,
					 setup_echo_srv_tcp_ipv4,
					 teardown),
	};

	rc = run_tests(tests);

	return rc;
}
