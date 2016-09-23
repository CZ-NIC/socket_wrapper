#include "torture.h"

#include <errno.h>
#include <stdio.h>
#include <cmocka.h>
#include <unistd.h>
#include <stdlib.h>

#define MAX_SOCKETS 4

static int setup(void **state)
{
	int ret;
	char str[10];

	torture_setup_socket_dir(state);

	ret = snprintf(str, 10, "%d", MAX_SOCKETS);
	if (ret < 0) {
		return ret;
	}

	ret = setenv("SOCKET_WRAPPER_MAX_SOCKETS", str, 1);

	return 0;
}

static int teardown(void **state)
{
	torture_teardown_socket_dir(state);

	return 0;
}

static int _socket(int *_s)
{
	int s;

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (s < 0) {
		return -1;
	}

	*_s = s;

	return 0;
}

static void test_max_sockets(void **state)
{
	int rc;
	int s[MAX_SOCKETS+1] = { 0 };
	int i;

	(void) state; /* unused */

	for (i = 0; i < MAX_SOCKETS; i++) {
		rc = _socket(&s[i]);
		assert_return_code(rc, errno);
	}

	/* no free space for sockets left */
	rc = _socket(&s[MAX_SOCKETS]);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, ENOMEM);

	/* closing a socket frees up space */
	close(s[0]);
	rc = _socket(&s[0]);
	assert_return_code(rc, errno);

	/* but just one */
	rc = _socket(&s[MAX_SOCKETS]);
	assert_int_equal(rc, -1);
	assert_int_equal(errno, ENOMEM);

	for (i = 0; i < MAX_SOCKETS; i++) {
		close(s[i]);
	}
}

int main(void) {
	int rc;

	const struct CMUnitTest max_sockets_tests[] = {
		cmocka_unit_test_setup_teardown(test_max_sockets,
						setup, teardown),
	};

	rc = cmocka_run_group_tests(max_sockets_tests, NULL, NULL);

	return rc;
}
