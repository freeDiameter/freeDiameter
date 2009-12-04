/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@nict.go.jp>							 *
*													 *
* Copyright (c) 2009, WIDE Project and NICT								 *
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

#include "fD.h"

/* Merge information into a list of apps */
int fd_app_merge(struct fd_list * list, application_id_t aid, vendor_id_t vid, int auth, int acct)
{
	struct fd_list * li;
	int skip = 0;
	
	/* List is ordered by appid. Avoid duplicates */
	for (li = list; li->next != list; li = li->next) {
		struct fd_app * na = (struct fd_app *)(li->next);
		if (na->appid < aid)
			continue;
		
		if (na->appid > aid)
			break;
		
		/* Otherwise, we merge with existing entry -- ignore vendor id in this case */
		skip = 1;
		
		if (auth)
			na->flags.auth = 1;
		if (acct)
			na->flags.acct = 1;
		break;
	}
	
	if (!skip) {			
		struct fd_app  * new = NULL;

		CHECK_MALLOC( new = malloc(sizeof(struct fd_app)) );
		memset(new, 0, sizeof(struct fd_app));
		fd_list_init(&new->chain, NULL);
		new->flags.auth = (auth ? 1 : 0);
		new->flags.acct = (acct ? 1 : 0);
		new->vndid = vid;
		new->appid = aid;
		fd_list_insert_after(li, &new->chain);
	}
	
	return 0;
}

/* Check if a given application id is in a list */
int fd_app_check(struct fd_list * list, application_id_t aid, struct fd_app **detail)
{
	struct fd_list * li;
	
	TRACE_ENTRY("%p %d %p", list, aid, detail);
	CHECK_PARAMS(list && detail);
	
	*detail = NULL;
	
	/* Search in the list */
	for (li = list->next; li != list; li = li->next) {
		struct fd_app * a = (struct fd_app *)li;
		if (a->appid < aid)
			continue;
		
		if (a->appid == aid)
			*detail = a;
		break;
	}
	
	return 0;
}
