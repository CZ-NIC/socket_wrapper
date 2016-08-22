#include "torture.h"

#include <cmocka.h>
#include <unistd.h>

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

static void test_dup2_existing_open_fd(void **state)
{
	int s, dup_s;

	(void) state; /* unused */

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	assert_int_not_equal(s, -1);

	/*
	 * Here we try to duplicate the existing socket fd to itself
	 * and as per man page for dup2() it must return the already
	 * open fd without any failure.
	 */
	dup_s = dup2(s, s);
	assert_int_equal(dup_s, s);

	close(s);
}

int main(void) {
	int rc;

	const struct CMUnitTest tcp_dup2_tests[] = {
		cmocka_unit_test(test_dup2_existing_open_fd),
	};

	rc = cmocka_run_group_tests(tcp_dup2_tests, setup, teardown);

	return rc;
}
