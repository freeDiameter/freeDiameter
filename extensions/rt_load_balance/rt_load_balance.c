/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Thomas Klausner <tk@giga.or.at>                                                                *
*                                                                                                        *
* Copyright (c) 2013, 2014 Thomas Klausner                                                               *
* All rights reserved.                                                                                   *
*                                                                                                        *
* Written under contract by nfotex IT GmbH, http://nfotex.com/                                           *
*                                                                                                        *
* Redistribution and use of this software in source and binary forms, with or without modification, are  *
* permitted provided that the following conditions are met:                                              *
*                                                                                                        *
* * Redistributions of source code must retain the above                                                 *
*   copyright notice, this list of conditions and the                                                    *
*   following disclaimer.                                                                                *
*                                                                                                        *
* * Redistributions in binary form must reproduce the above                                              *
*   copyright notice, this list of conditions and the                                                    *
*   following disclaimer in the documentation and/or other                                               *
*   materials provided with the distribution.                                                            *
*                                                                                                        *
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED *
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A *
* PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR *
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT     *
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS    *
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR *
* TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF   *
* ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                                                             *
*********************************************************************************************************/

#include <freeDiameter/extension.h>
#include <limits.h>
#include <time.h>

struct best_candidate_entry {
    long load;
    struct rtd_candidate *cand;
};

static unsigned int seed;

#define MODULE_NAME "rt_load_balance"

/*
 * Load balancing extension. Send request to least-loaded node: Add a
 * score of 1 to the least loaded candidates among those with highest
 * score.
 */

/* The callback for load balancing the requests across the peers */
static int rt_load_balancing(void * cbdata, struct msg ** pmsg, struct fd_list * candidates)
{
	struct fd_list *lic;
	struct msg * msg = *pmsg;
	int max_score = -1;
	int max_score_count = 0;

	TRACE_ENTRY("%p %p %p", cbdata, msg, candidates);

	CHECK_PARAMS(msg && candidates);


	/* Check if it is worth processing the message */
	if (FD_IS_LIST_EMPTY(candidates))
		return 0;

	/* find out maximal score and how many candidates have it */
	for (lic = candidates->next; lic != candidates; lic = lic->next) {
		struct rtd_candidate * cand = (struct rtd_candidate *) lic;
		if (max_score < cand->score) {
			max_score = cand->score;
			max_score_count = 1;
		}
		else if (cand->score == max_score) {
			max_score_count++;
		}
	}

	if (max_score_count > 0) {
		/* find out minimum load among those with maximal
		 * score, and how many candidates have it */
		struct best_candidate_entry best_candidates[max_score_count];
		long min_load = LONG_MAX;
		int min_load_count = 0;
		int j;

		for (j = 0, lic = candidates->next; lic != candidates; lic = lic->next) {
			struct rtd_candidate * cand = (struct rtd_candidate *) lic;
			if (cand->score == max_score) {
				long to_receive, to_send, load;
				struct peer_hdr *peer;
				CHECK_FCT(fd_peer_getbyid(cand->diamid, cand->diamidlen, 0, &peer));
				CHECK_FCT(fd_peer_get_load_pending(peer, &to_receive, &to_send));
				load = to_receive + to_send;

				best_candidates[j].cand = cand;
				best_candidates[j].load = load;
				j++;

				if (min_load > load) {
					min_load = load;
					min_load_count = 1;
				} else if (min_load == load) {
					min_load_count++;
				}
			}
		}

		/* increase score by 1 for all entries with minimum
		 * load, and further increase by 1 for one randomly
		 * chosen candidate */
		if (min_load_count > 0) {
			int lucky_candidate = rand_r(&seed) % min_load_count;
			int i;
			for (j = 0, i = 0; j < max_score_count; j++) {
				struct best_candidate_entry *entry = best_candidates+j;
				if (entry->load == min_load) {
					struct rtd_candidate *cand = entry->cand;
					int old_score = cand->score;
					cand->score++;
					TRACE_DEBUG(FULL, "%s: boosting peer `%.*s', score was %d, now %d; load was %ld", MODULE_NAME, (int)cand->diamidlen, cand->diamid, old_score, cand->score, entry->load);

					if (i == lucky_candidate) {
						cand->score++;
						TRACE_DEBUG(FULL, "%s: boosting lucky peer `%.*s', score was %d, now %d; load was %ld", MODULE_NAME, (int)cand->diamidlen, cand->diamid, old_score, cand->score, entry->load);
					}
					i++;
				}
			}
		}
	}

	return 0;
}

/* handler */
static struct fd_rt_out_hdl * rt_load_balancing_hdl = NULL;

/* entry point */
static int rt_load_balance_entry(char * conffile)
{
	/* Register the callback */
	CHECK_FCT(fd_rt_out_register(rt_load_balancing, NULL, 10, &rt_load_balancing_hdl));

	seed = (unsigned int)time(NULL);

	TRACE_DEBUG(INFO, "Extension 'Load Balancing' initialized");
	return 0;
}

/* Unload */
void fd_ext_fini(void)
{
	/* Unregister the callbacks */
	CHECK_FCT_DO(fd_rt_out_unregister(rt_load_balancing_hdl, NULL), /* continue */);
	return ;
}

EXTENSION_ENTRY(MODULE_NAME, rt_load_balance_entry);
