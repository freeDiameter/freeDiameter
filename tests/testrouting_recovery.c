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

/* Drive the routing step synchronously, without network peers or worker timing. */
#include "../libfdcore/routing_dispatch.c"

static struct msg *held, *held_extra;
static int calls, decline, scored;
static struct fd_rt_out_recovery_hdl *handler;
static pthread_mutex_t test_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t test_cond = PTHREAD_COND_INITIALIZER;
static int block_callback, callback_entered, release_callback, unregister_started, unregistered;

static struct msg *request(void)
{
	struct msg *msg;
	struct msg_hdr *hdr;
	CHECK(0, fd_msg_new(NULL, 0, &msg));
	CHECK(0, fd_msg_hdr(msg, &hdr));
	hdr->msg_flags = CMD_FLAG_REQUEST;
	hdr->msg_code = 271;
	hdr->msg_appl = 3;
	return msg;
}

static int recover(void *data, struct msg **msg)
{
	CHECK(&calls, data);
	CHECK(0, pthread_mutex_lock(&test_lock));
	calls++;
	if (block_callback) {
		callback_entered++;
		CHECK(0, pthread_cond_broadcast(&test_cond));
		while (!release_callback)
			CHECK(0, pthread_cond_wait(&test_cond, &test_lock));
	}
	if (!decline) {
		if (!held)
			held = *msg;
		else
			held_extra = *msg;
		*msg = NULL;
	}
	CHECK(0, pthread_mutex_unlock(&test_lock));
	return decline ? EIO : 0;
}

static void check_unable(void)
{
	struct msg *msg;
	struct msg_hdr *hdr;
	struct avp *avp;
	struct avp_hdr *ahdr;
	struct dict_object *model;
	CHECK(0, fd_fifo_tryget(fd_g_incoming, &msg));
	fd_work_done(NULL);
	CHECK(0, fd_msg_hdr(msg, &hdr));
	CHECK(0, hdr->msg_flags & CMD_FLAG_REQUEST);
	CHECK(0, fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME,
			       "Result-Code", &model, ENOENT));
	CHECK(0, fd_msg_search_avp(msg, model, &avp));
	CHECK(1, avp != NULL);
	CHECK(0, fd_msg_avp_hdr(avp, &ahdr));
	CHECK(3002, ahdr->avp_value->u32);
	CHECK(0, fd_msg_free(msg));
}

static int reject_candidates(void *data, struct msg **msg, struct fd_list *list)
{
	struct fd_list *li;
	(void)data;
	(void)msg;
	for (li = list->next; li != list; li = li->next) {
		struct rtd_candidate *c = (struct rtd_candidate *)li;
		CHECK(0, strcmp((char *)c->diamid, "new.example"));
		c->score = -1;
		scored++;
	}
	return 0;
}

static void unregister_on_error(enum fd_hook_type type, struct msg *msg, struct peer_hdr *peer,
				void *other, struct fd_hook_permsgdata *pmd, void *data)
{
	(void)type;
	(void)msg;
	(void)peer;
	(void)other;
	(void)pmd;
	(void)data;
	CHECK(0, fd_rt_out_recovery_unregister(handler, NULL));
}

static void *route_thread(void *unused)
{
	(void)unused;
	CHECK(0, msg_rt_out(request()));
	return NULL;
}

static void *unregister_thread(void *unused)
{
	(void)unused;
	CHECK(0, pthread_mutex_lock(&test_lock));
	unregister_started = 1;
	CHECK(0, pthread_cond_broadcast(&test_cond));
	CHECK(0, pthread_mutex_unlock(&test_lock));
	CHECK(0, fd_rt_out_recovery_unregister(handler, NULL));
	CHECK(0, pthread_mutex_lock(&test_lock));
	unregistered = 1;
	CHECK(0, pthread_cond_broadcast(&test_cond));
	CHECK(0, pthread_mutex_unlock(&test_lock));
	return NULL;
}

int main(int argc, char **argv)
{
	struct fd_rt_out_recovery_hdl *duplicate = NULL;
	struct fd_rt_out_hdl *score_handler;
	struct fd_hook_hdl *error_hook;
	struct msg *msg;
	struct rt_data *rtd;
	struct fd_list *candidates;
	struct fd_peer peers[4];
	const char *names[] = { "failed.example", "origin.example", "loop.example", "new.example" };
	struct avp *avp;
	struct dict_object *model;
	union avp_value value;
	pthread_t routing[2], unregistering;
	struct timespec deadline;
	void *data;
	int i, attempts;

	INIT_FD();
	fd_g_config->cnf_diamid = strdup("local.example");
	fd_g_config->cnf_diamid_len = strlen(fd_g_config->cnf_diamid);
	fd_g_config->cnf_diamrlm = strdup("example");
	fd_g_config->cnf_diamrlm_len = strlen(fd_g_config->cnf_diamrlm);
	CHECK(0, fd_msg_init());
	CHECK(0, fd_queues_init());

	/* No opt-in preserves the original error; declining does too. */
	CHECK(0, msg_rt_out(request()));
	check_unable();
	CHECK(0, fd_rt_out_recovery_register(recover, &calls, &handler));
	CHECK(EEXIST, fd_rt_out_recovery_register(recover, NULL, &duplicate));
	decline = 1;
	CHECK(0, msg_rt_out(request()));
	CHECK(1, calls);
	check_unable();
	decline = 0;

	/* Asynchronous ownership and explicit original failure. */
	CHECK(0, msg_rt_out(request()));
	CHECK(2, calls);
	CHECK(1, held != NULL);
	CHECK(0, fd_rt_out_recovery_resume(&held, 0));
	CHECK(1, held == NULL);
	check_unable();

	/* A resumed request sees new peers, never error origins or Route-Records. */
	CHECK(0, msg_rt_out(request()));
	CHECK(0, fd_msg_rt_get(held, &rtd));
	CHECK(0, fd_rtd_error_add(rtd, (DiamId_t)"FAILED.example", strlen(names[0]),
				(uint8_t *)"ORIGIN.example", strlen(names[1]), 3002, NULL, NULL));
	CHECK(0, fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME,
			       "Route-Record", &model, ENOENT));
	CHECK(0, fd_msg_avp_new(model, 0, &avp));
	value.os.data = (uint8_t *)"LOOP.example";
	value.os.len = strlen(names[2]);
	CHECK(0, fd_msg_avp_setvalue(avp, &value));
	CHECK(0, fd_msg_avp_add(held, MSG_BRW_LAST_CHILD, avp));
	memset(peers, 0, sizeof(peers));
	for (i = 0; i < 4; i++) {
		peers[i].p_hdr.info.pi_diamid = (DiamId_t)names[i];
		peers[i].p_hdr.info.pi_diamidlen = strlen(names[i]);
		fd_list_init(&peers[i].p_actives, &peers[i]);
		fd_list_insert_before(&fd_g_activ_peers, &peers[i].p_actives);
	}
	CHECK(0, fd_rtd_get_nb_attempts(rtd, &attempts));
	CHECK(1, attempts);
	CHECK(0, fd_rt_out_recovery_resume(&held, 1));
	CHECK(1, held == NULL);
	CHECK(0, fd_rtd_get_nb_attempts(rtd, &attempts));
	CHECK(1, attempts);
	fd_rtd_candidate_extract(rtd, &candidates, 0);
	CHECK(1, candidates->next == candidates->prev && candidates->next != candidates);
	CHECK(0, strcmp((char *)((struct rtd_candidate *)candidates->next)->diamid, "new.example"));
	for (i = 0; i < 4; i++)
		fd_list_unlink(&peers[i].p_actives);
	CHECK(0, fd_rt_out_register(reject_candidates, NULL, 0, &score_handler));
	CHECK(0, fd_fifo_tryget(fd_g_outgoing, &msg));
	fd_work_done(NULL);
	CHECK(0, msg_rt_out(msg));
	CHECK(1, scored);
	CHECK(3, calls); /* Failed retry must not re-enter recovery. */
	check_unable();
	CHECK(0, fd_rt_out_unregister(score_handler, NULL));

	/* Concurrent callbacks are allowed; unregister must wait for both. */
	block_callback = 1;
	CHECK(0, pthread_create(&routing[0], NULL, route_thread, NULL));
	CHECK(0, pthread_create(&routing[1], NULL, route_thread, NULL));
	CHECK(0, pthread_mutex_lock(&test_lock));
	while (callback_entered != 2)
		CHECK(0, pthread_cond_wait(&test_cond, &test_lock));
	CHECK(0, pthread_create(&unregistering, NULL, unregister_thread, NULL));
	while (!unregister_started)
		CHECK(0, pthread_cond_wait(&test_cond, &test_lock));
	CHECK(0, clock_gettime(CLOCK_REALTIME, &deadline));
	deadline.tv_sec++;
	while (!unregistered) {
		int ret = pthread_cond_timedwait(&test_cond, &test_lock, &deadline);
		if (ret == ETIMEDOUT)
			break;
		CHECK(0, ret);
	}
	CHECK(0, unregistered);
	release_callback = 1;
	CHECK(0, pthread_cond_broadcast(&test_cond));
	CHECK(0, pthread_mutex_unlock(&test_lock));
	CHECK(0, pthread_join(routing[0], NULL));
	CHECK(0, pthread_join(routing[1], NULL));
	CHECK(0, pthread_join(unregistering, NULL));
	CHECK(1, unregistered);
	CHECK(0, fd_rt_out_recovery_resume(&held, 0));
	check_unable();
	CHECK(0, fd_rt_out_recovery_resume(&held_extra, 0));
	check_unable();
	CHECK(0, msg_rt_out(request()));
	check_unable();
	CHECK(5, calls);

	/* Resume must not retain the callback lock across core/user error hooks. */
	CHECK(0, fd_rt_out_recovery_register(recover, &calls, &handler));
	CHECK(0, msg_rt_out(request()));
	CHECK(0, fd_hook_register(HOOK_MASK(HOOK_MESSAGE_ROUTING_ERROR),
				 unregister_on_error, NULL, NULL, &error_hook));
	CHECK(0, fd_rt_out_recovery_resume(&held, 0));
	check_unable();
	CHECK(0, fd_hook_unregister(error_hook));
	CHECK(6, calls);

	CHECK(0, fd_rt_out_recovery_register(recover, &calls, &handler));
	msg = request();
	fd_work_quiesce();
	CHECK(ESHUTDOWN, fd_rt_out_recovery_resume(&msg, 0));
	CHECK(1, msg != NULL);
	CHECK(0, fd_msg_free(msg));
	CHECK(0, msg_rt_out(request()));
	CHECK(6, calls);
	check_unable();
	CHECK(0, fd_rt_out_recovery_unregister(handler, &data));
	CHECK(&calls, data);
	CHECK(ESHUTDOWN, fd_rt_out_recovery_register(recover, &calls, &handler));
	CHECK(0, fd_rtdisp_fini());
	PASSTEST();
}
