#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

static void test_sendmsg_recvmsg_fd(void **state)
{
	int sv[2];
	int child_fd, parent_fd;
	int rc;
	pid_t pid;

	(void) state; /* unused */

	rc = socketpair(AF_LOCAL, SOCK_STREAM, 0, sv);
	assert_int_not_equal(rc, -1);

	parent_fd = sv[0];
	child_fd = sv[1];

	pid = fork();
	assert_int_not_equal(pid, -1);

	if (pid == 0) {
		/* Child */
		struct msghdr child_msg;
		char cmsgbuf[CMSG_SPACE(sizeof(int))];
		struct cmsghdr *cmsg;
		int rcv_fd;
		char buf[8];
		int i;

		memset(&child_msg, 0, sizeof(child_msg));
		child_msg.msg_control = cmsgbuf;
		child_msg.msg_controllen = sizeof(cmsgbuf);

		do {
			errno = 0;
			rc = recvmsg(child_fd, &child_msg, 0);
		} while (errno == EAGAIN || errno == EWOULDBLOCK);
		assert_int_not_equal(rc, -1);

		cmsg = CMSG_FIRSTHDR(&child_msg);
		assert_non_null(cmsg);
		assert_int_equal(cmsg->cmsg_type, SCM_RIGHTS);

		memcpy(&rcv_fd, CMSG_DATA(cmsg), sizeof(rcv_fd));
		assert_int_not_equal(rcv_fd, -1);

		rc = read(rcv_fd, buf, sizeof(buf));
		assert_int_not_equal(rc, -1);
		for (i = 0; i < 8; i++) {
			assert_int_equal(buf[i], 0);
		}
		exit(0);
	} else {
		/* Parent */
		int pass_fd;
		struct msghdr parent_msg;
		struct cmsghdr *cmsg;
		char cmsgbuf[CMSG_SPACE(sizeof(pass_fd))];
		char byte = '!';
		struct iovec iov;
		int cs;

		pass_fd = open("/dev/zero", O_RDONLY);
		assert_int_not_equal(pass_fd, -1);

		iov.iov_base = &byte;
		iov.iov_len = 1;

		memset(&parent_msg, 0, sizeof(parent_msg));
		parent_msg.msg_iov = &iov;
		parent_msg.msg_iovlen = 1;
		parent_msg.msg_control = cmsgbuf;
		parent_msg.msg_controllen = sizeof(cmsgbuf);

		cmsg = CMSG_FIRSTHDR(&parent_msg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(sizeof(pass_fd));

		memcpy(CMSG_DATA(cmsg), &pass_fd, sizeof(pass_fd));
		parent_msg.msg_controllen = cmsg->cmsg_len;

		rc = sendmsg(parent_fd, &parent_msg, 0);
		assert_int_not_equal(rc, -1);

		alarm(5);	    /* 5 seconds timeout for the child */
		rc = waitpid(pid, &cs, 0);
		assert_int_not_equal(rc, -1);
	}
}

int main(void) {
	int rc;

	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_sendmsg_recvmsg_fd),
	};

	rc = cmocka_run_group_tests(tests, NULL, NULL);

	return rc;
}
