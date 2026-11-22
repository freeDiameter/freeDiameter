/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: freeDiameter contributors                                                                      *
* Copyright (c) 2026, freeDiameter contributors                                                          *
* All rights reserved.                                                                                   *
* 													 *
* Redistribution and use of this software in source and binary forms, with or without modification, are  *
* permitted provided that the following conditions are met:						 *
* 													 *
* * Redistributions of source code must retain the above 						 *
*   copyright notice, this list of conditions and the 							 *
*   following disclaimer.										 *
*    													 *
* * Redistributions in binary form must reproduce the above copyright notice, this list of conditions,  *
*   and the following disclaimer in the documentation and/or other materials provided with the          *
*   distribution.											 *
*    													 *
* * Neither the name of the WIDE Project or NICT nor the 						 *
*   names of its contributors may be used to endorse or 							 *
*   promote products derived from this software without 							 *
*   specific prior written permission of WIDE Project and 							 *
*   NICT.												 *
*    													 *
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED *
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A *
* PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR *
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 	 *
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 	 *
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR *
* TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF   *
* ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.								 *
*********************************************************************************************************/

#include "tests.h"
#include "cnxctx.h"

static int expect_action(int error, enum fd_server_accept_action expected)
{
	enum fd_server_accept_action actual = fd_server_accept_error_action(error);
	if (actual != expected) {
		fprintf(stderr, "Unexpected action for errno %d: got %d, expected %d\n",
			error, actual, expected);
		return 0;
	}
	return 1;
}

static uint16_t get_free_port(void)
{
	struct sockaddr_in address;
	socklen_t length = sizeof(address);
	int sock = socket(AF_INET, SOCK_STREAM, 0);

	CHECK(1, sock >= 0);
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	CHECK(0, bind(sock, (sSA *)&address, sizeof(address)));
	CHECK(0, getsockname(sock, (sSA *)&address, &length));
	CHECK(0, close(sock));
	return ntohs(address.sin_port);
}

struct accept_test {
	struct cnxctx *listener;
	int result;
};

static void *accept_until_marker(void *arg)
{
	struct accept_test *test = arg;

	for (;;) {
		struct cnxctx *conn = fd_server_accept(test->listener);
		char marker;
		ssize_t received;
		int receive_error;

		if (!conn) {
			test->result = -1;
			return NULL;
		}

		do {
			received = recv(conn->cc_socket, &marker, sizeof(marker), 0);
		} while (received < 0 && errno == EINTR);
		receive_error = received < 0 ? errno : 0;
		fd_cnx_destroy(conn);

		if (received == 1 && marker == 'x') {
			test->result = 1;
			return NULL;
		}
		if (received == 0 || (received < 0 && receive_error == ECONNRESET))
			continue;

		test->result = -2;
		return NULL;
	}
}

int main(int argc, char *argv[])
{
	struct sockaddr_in address;
	struct accept_test test;
	pthread_t accept_thread;
	struct cnxctx *listener;
	int i;

	INIT_FD();
	CHECK(1, expect_action(EINTR, FD_SERVER_ACCEPT_RETRY));
	CHECK(1, expect_action(ECONNABORTED, FD_SERVER_ACCEPT_RETRY));
	CHECK(1, expect_action(ECONNRESET, FD_SERVER_ACCEPT_RETRY));
	CHECK(1, expect_action(EMFILE, FD_SERVER_ACCEPT_RETRY_DELAYED));
	CHECK(1, expect_action(ENFILE, FD_SERVER_ACCEPT_RETRY_DELAYED));
	CHECK(1, expect_action(ENOBUFS, FD_SERVER_ACCEPT_RETRY_DELAYED));
	CHECK(1, expect_action(ENOMEM, FD_SERVER_ACCEPT_RETRY_DELAYED));
	CHECK(1, expect_action(EBADF, FD_SERVER_ACCEPT_FATAL));

	listener = fd_cnx_serv_tcp(get_free_port(), AF_INET, NULL);
	CHECK(1, listener != NULL);
	CHECK(0, fd_cnx_serv_listen(listener));
	test.listener = listener;
	test.result = 0;

	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	{
		struct sockaddr_in local_address;
		socklen_t local_length = sizeof(local_address);
		CHECK(0, getsockname(listener->cc_socket, (sSA *)&local_address, &local_length));
		address.sin_port = local_address.sin_port;
	}

	/* Queue several resets before accepting; the listener backlog is five. */
	for (i = 0; i < 4; i++) {
		struct linger linger = { 1, 0 };
		int client = socket(AF_INET, SOCK_STREAM, 0);
		CHECK(1, client >= 0);
		CHECK(0, connect(client, (sSA *)&address, sizeof(address)));
		CHECK(0, setsockopt(client, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger)));
		CHECK(0, close(client));
	}
	CHECK(0, pthread_create(&accept_thread, NULL, accept_until_marker, &test));
	{
		int client = socket(AF_INET, SOCK_STREAM, 0);
		char marker = 'x';
		CHECK(1, client >= 0);
		CHECK(0, connect(client, (sSA *)&address, sizeof(address)));
		CHECK(sizeof(marker), send(client, &marker, sizeof(marker), 0));
		CHECK(0, close(client));
	}

	CHECK(0, pthread_join(accept_thread, NULL));
	CHECK(1, test.result == 1);
	fd_cnx_destroy(listener);
	PASSTEST();
}
