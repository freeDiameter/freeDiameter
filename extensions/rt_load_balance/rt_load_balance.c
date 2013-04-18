#include <freeDiameter/extension.h>

/* 
 * Load balancing extension. Send request to least-loaded node.
 */

/* The callback for load balancing the requests across the peers */
static int rt_load_balancing(void * cbdata, struct msg * msg, struct fd_list * candidates)
{
	struct fd_list *lic;
	
	TRACE_ENTRY("%p %p %p", cbdata, msg, candidates);
	
	CHECK_PARAMS(msg && candidates);
	
	/* Check if it is worth processing the message */
	if (FD_IS_LIST_EMPTY(candidates))
		return 0;

	/* load balancing */
	for (lic = candidates->next; lic != candidates; lic = lic->next) {
		struct rtd_candidate * cand = (struct rtd_candidate *) lic;
		struct peer_hdr *peer;
		long to_receive, to_send, load;
		int score;
		CHECK_FCT(fd_peer_getbyid(cand->diamid, cand->diamidlen, 0, &peer));
		CHECK_FCT(fd_peer_get_load_pending(peer, &to_receive, &to_send));
                load = to_receive + to_send;
		score = cand->score;
		if (load > 5)
			cand->score -= 5;
		else
			cand->score -= load;
		TRACE_DEBUG(INFO, "evaluated peer `%.*s', score was %d, now %d", (int)cand->diamidlen, cand->diamid, score, cand->score);
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

EXTENSION_ENTRY("rt_load_balance", rt_load_balance_entry);
