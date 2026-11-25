/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Copyright (c) 2026, freeDiameter contributors                                                          *
* All rights reserved.                                                                                  *
*                                                                                                       *
* Redistribution and use in source and binary forms, with or without modification, are permitted        *
* provided that the following conditions are met:                                                       *
* * Redistributions of source code must retain the above copyright notice, this list of conditions     *
*   and the following disclaimer.                                                                       *
* * Redistributions in binary form must reproduce the above copyright notice, this list of conditions   *
*   and the following disclaimer in the documentation and/or other materials provided with the          *
*   distribution.                                                                                       *
* * Neither the name of the copyright holder nor the names of its contributors may be used to endorse   *
*   or promote products derived from this software without specific prior written permission.           *
*                                                                                                       *
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED *
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A *
* PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR *
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT     *
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS     *
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR *
* TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF   *
* ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                                                             *
*********************************************************************************************************/

#include "tests.h"
#include <sys/wait.h>

static void stuck_hook(enum fd_hook_type type, struct msg *msg, struct peer_hdr *peer,
		       void *other, struct fd_hook_permsgdata *pmd, void *data)
{
	CHECK(0, pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL));
	if (data) {
		/* An ongoing indefinite shutdown must not acquire a later deadline. */
		CHECK(0, fd_core_shutdown_timeout(0));
		sleep(6);
		return;
	}
	for (;;)
		sleep(1);
}

static void child(int argc, char **argv, int stuck)
{
	struct fd_hook_hdl *hook;
	struct msg *msg;
	INIT_FD();
	CHECK(0, fd_queues_init());
	CHECK(EINVAL, fd_core_shutdown_timeout(-2));
	if (stuck) {
		CHECK(0, fd_hook_register(HOOK_MASK(HOOK_MESSAGE_DROPPED), stuck_hook,
					 stuck > 1 ? &stuck : NULL, NULL, &hook));
		CHECK(0, fd_msg_new(NULL, 0, &msg));
		CHECK(0, fd_work_post_msg(fd_g_outgoing, &msg));
	}
	if (stuck == 2) {
		CHECK(0, fd_core_shutdown());
	} else {
		CHECK(0, fd_core_shutdown_timeout(stuck == 3 ? -1 : 4));
	}
	CHECK(0, fd_core_wait_shutdown_complete());
	if (stuck == 1)
		_exit(2);
	/* Remain alive past the watchdog deadline to check that it was disarmed. */
	if (!stuck)
		sleep(6);
	_exit(0);
}

int main(int argc, char **argv)
{
	pid_t blocked, normal, indefinite, legacy;
	int status, logpipe[2];
	char logbuf[4096];
	ssize_t len;
	struct timespec start, end;

	CHECK(0, pipe(logpipe));
	CHECK(0, clock_gettime(CLOCK_MONOTONIC, &start));
	blocked = fork();
	CHECK(1, blocked >= 0);
	if (!blocked) {
		close(logpipe[0]);
		CHECK(STDERR_FILENO, dup2(logpipe[1], STDERR_FILENO));
		close(logpipe[1]);
		child(argc, argv, 1);
	}
	close(logpipe[1]);
	normal = fork();
	CHECK(1, normal >= 0);
	if (!normal) {
		close(logpipe[0]);
		child(argc, argv, 0);
	}
	indefinite = fork();
	CHECK(1, indefinite >= 0);
	if (!indefinite) {
		close(logpipe[0]);
		child(argc, argv, 3);
	}
	legacy = fork();
	CHECK(1, legacy >= 0);
	if (!legacy) {
		close(logpipe[0]);
		child(argc, argv, 2);
	}
	CHECK(blocked, waitpid(blocked, &status, 0));
	CHECK(1, WIFEXITED(status));
	CHECK(EXIT_FAILURE, WEXITSTATUS(status));
	CHECK(0, clock_gettime(CLOCK_MONOTONIC, &end));
	CHECK(1, end.tv_sec - start.tv_sec >= 4);
	CHECK(1, end.tv_sec - start.tv_sec <= 7);
	len = read(logpipe[0], logbuf, sizeof(logbuf) - 1);
	CHECK(1, len > 0);
	logbuf[len] = '\0';
	CHECK(1, strstr(logbuf, "shutdown exceeded 4 seconds; forcing process exit") != NULL);
	CHECK(0, close(logpipe[0]));
	CHECK(normal, waitpid(normal, &status, 0));
	CHECK(1, WIFEXITED(status));
	CHECK(0, WEXITSTATUS(status));
	CHECK(indefinite, waitpid(indefinite, &status, 0));
	CHECK(1, WIFEXITED(status));
	CHECK(0, WEXITSTATUS(status));
	CHECK(legacy, waitpid(legacy, &status, 0));
	CHECK(1, WIFEXITED(status));
	CHECK(0, WEXITSTATUS(status));
	return 0;
}
