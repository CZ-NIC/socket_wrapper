#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <string.h>

#include "config.h"

#include "socket_wrapper.c"

#ifdef HAVE_STRUCT_MSGHDR_MSG_CONTROL

/**
 * test wrap_sendmsg_filter_cmsghdr()
 *
 * Prepare a message with two cmsg:
 * - the first cmsg is a char buf with the string "Hello World"
 * - the second cmsg is a char buf with the string "!\n"
 *
 * Both cmsgs will be copied without modification by
 * wrap_sendmsg_filter_cmsghdr(), so we can check that the msg
 * controllen, the cmsg sizes and the payload are the same.
 *
 * We use an not existing cmsg_type which triggers cmsg copying.
 */
static void test_sendmsg_cmsg(void **state)
{
	int rc = 0;
	char byte = '!';
	struct iovec iov;
	struct msghdr msg = { 0 };
	struct cmsghdr *cmsg;
	char *cmsgbuf;
	int cmsgbuf_size;
	const char *s1 = "Hello World";
	const int s1_len = strlen(s1);
	const char *s2 = "!\n";
	const int s2_len = strlen(s2);
	uint8_t *cmbuf = NULL;
	size_t cmlen = 0;

	(void)state; /* unused */

	iov.iov_base = &byte;
	iov.iov_len = 1;

	/*
	 * Prepare cmsgbuf and msg
	 */
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	cmsgbuf_size = CMSG_SPACE(s1_len) + CMSG_SPACE(s2_len);
	cmsgbuf = calloc(cmsgbuf_size, sizeof(char));
	assert_non_null(cmsgbuf);
	msg.msg_control = cmsgbuf;
	msg.msg_controllen = cmsgbuf_size;

	/*
	 * Prepare first cmsg with string "Hello World"
	 */
	cmsg = CMSG_FIRSTHDR(&msg);
	assert_non_null(cmsg);

	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = ~0 - 1;
	cmsg->cmsg_len = CMSG_LEN(s1_len);
	memcpy(CMSG_DATA(cmsg), s1, s1_len);

	/*
	 * Prepare second cmsg with string "!\n"
	 */
	cmsg = CMSG_NXTHDR(&msg, cmsg);
	assert_non_null(cmsg);

	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = ~0 - 2;
	cmsg->cmsg_len = CMSG_LEN(s2_len);
	memcpy(CMSG_DATA(cmsg), s2, s2_len);

	/*
	 * Now call swrap_sendmsg_filter_cmsghdr() on the msg
	 */
	rc = swrap_sendmsg_filter_cmsghdr(&msg, &cmbuf, &cmlen);
	assert_return_code(rc, errno);
	assert_int_equal(cmlen, msg.msg_controllen);

	/*
	 * Insert filtered cmsgbug into msg and validate cmsgs.
	 */
	msg.msg_control = cmbuf;

	cmsg = CMSG_FIRSTHDR(&msg);
	assert_non_null(cmsg);
	assert_int_equal(cmsg->cmsg_len, CMSG_LEN(s1_len));
	assert_memory_equal(CMSG_DATA(cmsg), s1, s1_len);

	cmsg = CMSG_NXTHDR(&msg, cmsg);
	assert_non_null(cmsg);
	assert_int_equal(cmsg->cmsg_len, CMSG_LEN(s2_len));
	assert_memory_equal(CMSG_DATA(cmsg), s2, s2_len);

	free(cmbuf);
	free(cmsgbuf);
}
#endif

int main(void) {
	int rc;

	const struct CMUnitTest unit_tests[] = {
#ifdef HAVE_STRUCT_MSGHDR_MSG_CONTROL
		cmocka_unit_test(test_sendmsg_cmsg),
#endif
	};

	rc = cmocka_run_group_tests(unit_tests, NULL, NULL);

	return rc;
}
