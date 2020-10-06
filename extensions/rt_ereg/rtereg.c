/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@freediameter.net>							 *
*													 *
* Copyright (c) 2020, WIDE Project and NICT								 *
* All rights reserved.											 *
* 													 *
* Redistribution and use of this software in source and binary forms, with or without modification, are  *
* permitted provided that the following conditions are met:						 *
* 													 *
* * Redistributions of source code must retain the above 						 *
*   copyright notice, this list of conditions and the 							 *
*   following disclaimer.										 *
*    													 *
* * Redistributions in binary form must reproduce the above 						 *
*   copyright notice, this list of conditions and the 							 *
*   following disclaimer in the documentation and/or other						 *
*   materials provided with the distribution.								 *
* 													 *
* * Neither the name of the WIDE Project or NICT nor the 						 *
*   names of its contributors may be used to endorse or 						 *
*   promote products derived from this software without 						 *
*   specific prior written permission of WIDE Project and 						 *
*   NICT.												 *
* 													 *
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED *
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A *
* PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR *
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 	 *
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 	 *
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR *
* TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF   *
* ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.								 *
*********************************************************************************************************/

/*
 * This extension allows to perform some pattern-matching on an AVP
 * and send the message to a server accordingly.
 * See rt_ereg.conf.sample file for the format of the configuration file.
 */

#include <pthread.h>
#include <signal.h>

#include "rtereg.h"

static pthread_rwlock_t rte_lock;

#define MODULE_NAME "rt_ereg"

static char *rt_ereg_config_file;

/* The configuration structure */
struct rtereg_conf *rtereg_conf;
int rtereg_conf_size;

#ifndef HAVE_REG_STARTEND
static char * buf = NULL;
static size_t bufsz;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
#endif /* HAVE_REG_STARTEND */

static int rtereg_init(void);
static int rtereg_init_config(void);
static void rtereg_fini(void);

void rtereg_conf_free(struct rtereg_conf *config_struct, int config_size)
{
	int i, j;

    	/* Destroy the data */
	for (j=0; j<config_size; j++) {
		if (config_struct[j].rules) {
			for (i = 0; i < config_struct[j].rules_nb; i++) {
				free(config_struct[j].rules[i].pattern);
				free(config_struct[j].rules[i].server);
				regfree(&config_struct[j].rules[i].preg);
			}
		}
		free(config_struct[j].avps);
		free(config_struct[j].rules);
	}
	free(config_struct);
}

static int proceed(char * value, size_t len, struct fd_list * candidates, int conf)
{
	int i;

	for (i = 0; i < rtereg_conf[conf].rules_nb; i++) {
		/* Does this pattern match the value? */
		struct rtereg_rule * r = &rtereg_conf[conf].rules[i];
		int err = 0;
		struct fd_list * c;

		LOG_D("[rt_ereg] attempting pattern matching of '%.*s' with rule '%s'", (int)len, value, r->pattern);

		#ifdef HAVE_REG_STARTEND
		{
			regmatch_t pmatch[1];
			memset(pmatch, 0, sizeof(pmatch));
			pmatch[0].rm_so = 0;
			pmatch[0].rm_eo = len;
			err = regexec(&r->preg, value, 0, pmatch, REG_STARTEND);
		}
		#else /* HAVE_REG_STARTEND */
		{
			/* We have a 0-terminated string */
			err = regexec(&r->preg, value, 0, NULL, 0);
		}
		#endif /* HAVE_REG_STARTEND */

		if (err == REG_NOMATCH)
			continue;

		if (err != 0) {
			char * errstr;
			size_t bl;

			/* Error while compiling the regex */
			LOG_E("[rt_ereg] error while executing the regular expression '%s':", r->pattern);

			/* Get the error message size */
			bl = regerror(err, &r->preg, NULL, 0);

			/* Alloc the buffer for error message */
			CHECK_MALLOC( errstr = malloc(bl) );

			/* Get the error message content */
			regerror(err, &r->preg, errstr, bl);
			LOG_E("\t%s", errstr);

			/* Free the buffer, return the error */
			free(errstr);

			return (err == REG_ESPACE) ? ENOMEM : EINVAL;
		}

		/* From this point, the expression matched the AVP value */
		LOG_D("[rt_ereg] Match: '%s' to value '%.*s' => '%s' += %d", r->pattern, (int)len, value, r->server, r->score);

		for (c = candidates->next; c != candidates; c = c->next) {
			struct rtd_candidate * cand = (struct rtd_candidate *)c;

			if (strcmp(r->server, cand->diamid) == 0) {
				cand->score += r->score;
				break;
			}
		}
	};

	return 0;
}

static int find_avp(msg_or_avp *where, int conf_index, int level, struct fd_list * candidates)
{
	struct dict_object *what;
	struct dict_avp_data dictdata;
	struct avp *nextavp = NULL;
	struct avp_hdr *avp_hdr = NULL;

	/* iterate over all AVPs and try to find a match */
//	for (i = 0; i<rtereg_conf[j].level; i++) {
	if (level > rtereg_conf[conf_index].level) {
		LOG_E("internal error, dug too deep");
		return 1;
	}
	what = rtereg_conf[conf_index].avps[level];

	CHECK_FCT(fd_dict_getval(what, &dictdata));
	CHECK_FCT(fd_msg_browse(where, MSG_BRW_FIRST_CHILD, (void *)&nextavp, NULL));
	while (nextavp) {
		CHECK_FCT(fd_msg_avp_hdr(nextavp, &avp_hdr));
		if ((avp_hdr->avp_code == dictdata.avp_code) && (avp_hdr->avp_vendor == dictdata.avp_vendor)) {
			if (level != rtereg_conf[conf_index].level - 1) {
				LOG_D("[rt_ereg] found grouped AVP %d (vendor %d), digging deeper", avp_hdr->avp_code, avp_hdr->avp_vendor);
				CHECK_FCT(find_avp(nextavp, conf_index, level+1, candidates));
			} else {
				struct dictionary * dict;
				LOG_D("[rt_ereg] found AVP %d (vendor %d)", avp_hdr->avp_code, avp_hdr->avp_vendor);
				CHECK_FCT(fd_dict_getdict(what, &dict));
				CHECK_FCT_DO(fd_msg_parse_dict(nextavp, dict, NULL), /* nothing */);
				if (avp_hdr->avp_value != NULL) {
					LOG_A("avp_hdr->avp_value NOT NULL, matching");
#ifndef HAVE_REG_STARTEND
					int ret;

					/* Lock the buffer */
					CHECK_POSIX( pthread_mutex_lock(&mtx) );

					/* Augment the buffer if needed */
					if (avp_hdr->avp_value->os.len >= bufsz) {
						char *newbuf;
						CHECK_MALLOC_DO( newbuf = realloc(buf, avp_hdr->avp_value->os.len + 1),
							{ pthread_mutex_unlock(&mtx); return ENOMEM; } );
						/* Update buffer and buffer size */
						buf = newbuf;
						bufsz = avp_hdr->avp_value->os.len + 1;
					}

					/* Copy the AVP value */
					memcpy(buf, avp_hdr->avp_value->os.data, avp_hdr->avp_value->os.len);
					buf[avp_hdr->avp_value->os.len] = '\0';

					/* Now apply the rules */
					ret = proceed(buf, avp_hdr->avp_value->os.len, candidates, conf_index);

					CHECK_POSIX(pthread_mutex_unlock(&mtx));

					CHECK_FCT(ret);
#else /* HAVE_REG_STARTEND */
					CHECK_FCT( proceed((char *) avp_hdr->avp_value->os.data, avp_hdr->avp_value->os.len, candidates, conf_index) );
#endif /* HAVE_REG_STARTEND */
				}
			}
		}
		CHECK_FCT(fd_msg_browse(nextavp, MSG_BRW_NEXT, (void *)&nextavp, NULL));
	}

	return 0;
}

/* The callback called on new messages */
static int rtereg_out(void * cbdata, struct msg ** pmsg, struct fd_list * candidates)
{
	msg_or_avp *where;
	int j, ret;

	LOG_A("[rt_ereg] rtereg_out arguments: %p %p %p", cbdata, *pmsg, candidates);

	CHECK_PARAMS(pmsg && *pmsg && candidates);

	if (pthread_rwlock_rdlock(&rte_lock) != 0) {
		fd_log_notice("%s: read-lock failed, skipping handler", MODULE_NAME);
		return 0;
	}
	ret = 0;
	/* Check if it is worth processing the message */
	if (!FD_IS_LIST_EMPTY(candidates)) {
		/* Now search the AVPs in the message */

		for (j=0; j<rtereg_conf_size; j++) {
			where = *pmsg;
			LOG_D("[rt_ereg] iterating over AVP group %d", j);
			if ((ret=find_avp(where, j, 0, candidates)) != 0) {
				break;
			}
		}
	}
	if (pthread_rwlock_unlock(&rte_lock) != 0) {
		fd_log_notice("%s: read-unlock failed after rtereg_out, exiting", MODULE_NAME);
		exit(1);
	}

	return ret;
}

/* handler */
static struct fd_rt_out_hdl * rtereg_hdl = NULL;

static volatile int in_signal_handler = 0;

/* signal handler */
static void sig_hdlr(void)
{
	struct rtereg_conf *old_config;
	int old_config_size;

	if (in_signal_handler) {
		fd_log_error("%s: already handling a signal, ignoring new one", MODULE_NAME);
		return;
	}
	in_signal_handler = 1;

	if (pthread_rwlock_wrlock(&rte_lock) != 0) {
		fd_log_error("%s: locking failed, aborting config reload", MODULE_NAME);
		return;
	}

	/* save old config in case reload goes wrong */
	old_config = rtereg_conf;
	old_config_size = rtereg_conf_size;
	rtereg_conf = NULL;
	rtereg_conf_size = 0;

	if (rtereg_init_config() != 0) {
		fd_log_notice("%s: error reloading configuration, restoring previous configuration", MODULE_NAME);
		rtereg_conf = old_config;
		rtereg_conf_size = old_config_size;
	} else {
		rtereg_conf_free(old_config, old_config_size);
	}

	if (pthread_rwlock_unlock(&rte_lock) != 0) {
		fd_log_error("%s: unlocking failed after config reload, exiting", MODULE_NAME);
		exit(1);
	}

	fd_log_notice("%s: reloaded configuration, %d AVP group%s defined", MODULE_NAME, rtereg_conf_size, rtereg_conf_size != 1 ? "s" : "");

	in_signal_handler = 0;
}

/* entry point */
static int rtereg_entry(char * conffile)
{
	LOG_A("[rt_ereg] started with conffile '%p'", conffile);

	rt_ereg_config_file = conffile;

	if (rtereg_init() != 0) {
		return 1;
	}

	/* Register reload callback */
	CHECK_FCT(fd_event_trig_regcb(SIGUSR1, MODULE_NAME, sig_hdlr));

	fd_log_notice("%s: configured, %d AVP group%s defined", MODULE_NAME, rtereg_conf_size, rtereg_conf_size != 1 ? "s" : "");

	return 0;
}

static int rtereg_init_config(void)
{
	/* Initialize the configuration */
	if ((rtereg_conf=calloc(sizeof(*rtereg_conf), 1)) == NULL) {
		LOG_E("[rt_ereg] malloc failured");
		return 1;
	}
	rtereg_conf_size = 1;

	/* Parse the configuration file */
	CHECK_FCT( rtereg_conf_handle(rt_ereg_config_file) );

	return 0;
}


/* Load */
static int rtereg_init(void)
{
	int ret;

	pthread_rwlock_init(&rte_lock, NULL);

	if (pthread_rwlock_wrlock(&rte_lock) != 0) {
		fd_log_notice("%s: write-lock failed, aborting", MODULE_NAME);
		return EDEADLK;
	}

	if ((ret=rtereg_init_config()) != 0) {
		pthread_rwlock_unlock(&rte_lock);
		return ret;
	}

	if (pthread_rwlock_unlock(&rte_lock) != 0) {
		fd_log_notice("%s: write-unlock failed, aborting", MODULE_NAME);
		return EDEADLK;
	}

	/* Register the callback */
	CHECK_FCT( fd_rt_out_register( rtereg_out, NULL, 1, &rtereg_hdl ) );

	/* We're done */
	return 0;
}

/* Unload */
static void rtereg_fini(void)
{
	/* Unregister the cb */
	CHECK_FCT_DO( fd_rt_out_unregister ( rtereg_hdl, NULL ), /* continue */ );

#ifndef HAVE_REG_STARTEND
	free(buf);
	buf = NULL;
	bufsz = 0;
#endif /* HAVE_REG_STARTEND */

	if (pthread_rwlock_wrlock(&rte_lock) != 0) {
		fd_log_error("%s: write-locking failed in fini, giving up", MODULE_NAME);
		return;
	}
	/* Destroy the data */
	rtereg_conf_free(rtereg_conf, rtereg_conf_size);
	rtereg_conf = NULL;
	rtereg_conf_size = 0;

	if (pthread_rwlock_unlock(&rte_lock) != 0) {
		fd_log_error("%s: write-unlocking failed in fini", MODULE_NAME);
		return;
	}

	/* Done */
	return ;
}

void fd_ext_fini(void)
{
	rtereg_fini();
}

EXTENSION_ENTRY(MODULE_NAME, rtereg_entry);
