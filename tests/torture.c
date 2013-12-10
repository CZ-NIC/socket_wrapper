/*
 * Copyright (C) Andreas Schneider 2013 <asn@samba.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include "torture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TORTURE_SOCKET_DIR "/tmp/test_socket_wrapper_XXXXXX"
#define TORTURE_ECHO_SRV_PIDFILE "echo_srv.pid"
#define TORTURE_ECHO_SRV_IPV4 "127.0.0.10"
#define TORTURE_ECHO_SRV_IPV6 "::10"

void torture_setup_socket_dir(void **state)
{
	struct torture_state *s;
	const char *p;
	size_t len;

	s = malloc(sizeof(struct torture_state));
	assert_non_null(s);

	s->socket_dir = strdup(TORTURE_SOCKET_DIR);
	assert_non_null(s->socket_dir);

	p = mkdtemp(s->socket_dir);
	assert_non_null(p);

	len = strlen(p) + 1 + strlen(TORTURE_ECHO_SRV_PIDFILE) + 1;

	s->srv_pidfile = malloc(len);
	assert_non_null(s->srv_pidfile);

	snprintf(s->srv_pidfile, len, "%s/%s", p, TORTURE_ECHO_SRV_PIDFILE);

	setenv("SOCKET_WRAPPER_DIR", p, 1);
	setenv("SOCKET_WRAPPER_DEFAULT_IFACE", "21", 1);

	*state = s;
}

void torture_setup_echo_srv_udp_ipv4(void **state)
{
	struct torture_state *s;
	char start_echo_srv[1024] = {0};
	int rc;

	torture_setup_socket_dir(state);

	s = *state;

	snprintf(start_echo_srv, sizeof(start_echo_srv),
		 "%s/tests/echo_srv -b %s -D -u --pid %s",
		 BINARYDIR, TORTURE_ECHO_SRV_IPV4, s->srv_pidfile);

	rc = system(start_echo_srv);
	assert_int_equal(rc, 0);

	sleep(1);
}
