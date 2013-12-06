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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <signal.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>

#define ECHO_SRV_IP "127.0.0.10"
#define ECHO_SRV_PORT 7

#define ECHO_SRV_PIDFILE "echo_srv.pid"

struct test_opts {
	char *socket_wrapper_dir;
	char *pidfile;
};

static void setup(void **state)
{
	char test_tmpdir[256];
	struct test_opts *o;
	size_t len;
	const char *p;

	o = malloc(sizeof(struct test_opts));
	assert_non_null(o);

	snprintf(test_tmpdir, sizeof(test_tmpdir), "/tmp/test_socket_wrapper_XXXXXX");

	p = mkdtemp(test_tmpdir);
	assert_non_null(p);

	o->socket_wrapper_dir = strdup(p);
	assert_non_null(o->socket_wrapper_dir);

	len = strlen(p) + 1 + strlen(ECHO_SRV_PIDFILE) + 1;

	o->pidfile = malloc(len);
	assert_non_null(o->pidfile);

	snprintf(o->pidfile, len, "%s/%s", p, ECHO_SRV_PIDFILE);

	setenv("SOCKET_WRAPPER_DIR", p, 1);
	setenv("SOCKET_WRAPPER_DEFAULT_IFACE", "21", 1);

	*state = o;
}

static void setup_echo_srv_udp(void **state)
{
	struct test_opts *o;
	char start_echo_srv[1024] = {0};
	int rc;

	setup(state);
	o = *state;

	snprintf(start_echo_srv, sizeof(start_echo_srv),
		 "%s/tests/echo_srv -b %s -D -u --pid %s",
		 BINARYDIR, ECHO_SRV_IP, o->pidfile);

	rc = system(start_echo_srv);
	assert_int_equal(rc, 0);

	sleep(1);
}

static void teardown(void **state)
{
	struct test_opts *o = *state;
	char remove_cmd[1024] = {0};
	int rc;

	(void) state; /* unused */

	snprintf(remove_cmd, sizeof(remove_cmd), "rm -rf %s", o->socket_wrapper_dir);

	rc = system(remove_cmd);
	if (rc < 0) {
		fprintf(stderr, "%s failed: %s", remove_cmd, strerror(errno));
	}

	free(o->socket_wrapper_dir);
	free(o->pidfile);
	free(o);
}

static void teardown_echo_srv_udp(void **state)
{
	struct test_opts *o = *state;
	char buf[8] = {0};
	long int tmp;
	ssize_t rc;
	pid_t pid;
	int fd;

	/* read the pidfile */
	fd = open(o->pidfile, O_RDONLY);
	if (fd < 0) {
		goto done;
	}

	rc = read(fd, buf, sizeof(buf));
	close(fd);
	if (rc <= 0) {
		goto done;
	}

	buf[sizeof(buf) - 1] = '\0';

	tmp = strtol(buf, NULL, 10);
	if (tmp == 0 || tmp > 0xFFFF || errno == ERANGE) {
		goto done;
	}

	pid = (pid_t)(tmp & 0xFFFF);

	/* kill daemon */
	kill(pid, SIGTERM);

done:
	teardown(state);
}

static void test_sendto_recvfrom_ipv4(void **state)
{
	struct sockaddr_in sin;
	socklen_t slen = sizeof(struct sockaddr_in);
	ssize_t ret;
	int rc;
	int i;
	int s;

	(void) state; /* unused */

	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	assert_int_not_equal(s, -1);

	ZERO_STRUCT(sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(ECHO_SRV_PORT);

	rc = inet_aton(ECHO_SRV_IP, &sin.sin_addr);
	assert_int_equal(rc, 1);

	for (i = 0; i < 10; i++) {
		char send_buf[64] = {0};
		char recv_buf[64] = {0};
		struct sockaddr_in cli_in;
		socklen_t clen;

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
			       (struct sockaddr *)&cli_in,
			       &clen);

		assert_memory_equal(send_buf, recv_buf, sizeof(send_buf));
	}
}

int main(void) {
	int rc;

	const UnitTest tests[] = {
		unit_test_setup_teardown(test_sendto_recvfrom_ipv4, setup_echo_srv_udp, teardown_echo_srv_udp),
	};

	rc = run_tests(tests);

	return rc;
}
