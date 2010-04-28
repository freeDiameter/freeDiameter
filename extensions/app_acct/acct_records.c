/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@nict.go.jp>							 *
*													 *
* Copyright (c) 2010, WIDE Project and NICT								 *
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

/* Functions to create a list of AVPs according to the configuration file */

#include "app_acct.h"

/* Prepare a list of acct_record entries from the configuration, without mapping any value at this time */
int acct_rec_prepare(struct acct_record_list * records)
{
	struct fd_list * li;
	TRACE_ENTRY("%p", records);
	CHECK_PARAMS( records && acct_config );
	
	/* Prepare the records structure */
	memset(records, 0, sizeof(struct acct_record_list));
	fd_list_init(&records->all, records);
	fd_list_init(&records->unmaped, records);
	
	/* for each entry in the configuration */
	for (li = acct_config->avps.next; li != &acct_config->avps; li = li->next) {
		struct acct_conf_avp * a = (struct acct_conf_avp *)li;
		struct acct_record_item * new;
		int i = a->multi ? 1 : 0;
		/* Create as many records as the 'multi' parameter requires */
		do {
			CHECK_MALLOC( new = malloc(sizeof(struct acct_record_item)) );
			memset(new, 0, sizeof(struct acct_record_item));
			fd_list_init(&new->chain, new);
			fd_list_init(&new->unmapd, new);
			new->param = a;
			new->index = i;
			fd_list_insert_before(&records->all, &new->chain);
			fd_list_insert_before(&records->unmaped, &new->unmapd);
			records->nball++;
			records->nbunmap++;
			i++;
		} while (i <= a->multi);
	}
	
	return 0;
}

int acct_rec_map(struct acct_record_list * records, struct msg * msg)
{
	TRACE_ENTRY("%p %p", records, msg);
	
	/* For each AVP in the message, search if we have a corresponding unmap'd record */
	
	return ENOTSUP;
}
