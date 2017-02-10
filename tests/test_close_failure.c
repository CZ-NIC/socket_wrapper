#include "torture.h"

#include <errno.h>
#include <stdio.h>
#include <cmocka.h>
#include <unistd.h>
#include <stdlib.h>

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

static void test_close_failure(void **state)
{
	int s;
	int rc;

	(void) state; /* unused */
	(void) s; /*set but not used */

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	assert_int_not_equal(rc, -1);

	/* Do not close the socket here so that destructor
	 * handles it and no hang should be observed.*/
}

int main(void) {
	int rc;

	const struct CMUnitTest close_failure_tests[] = {
		cmocka_unit_test_setup_teardown(test_close_failure,
						setup, teardown),
	};

	rc = cmocka_run_group_tests(close_failure_tests, NULL, NULL);

	return rc;
}
