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

/* This file contains the definitions for internal use in the libfreeDiameter library */

#ifndef _LIBFD_H
#define _LIBFD_H

#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfreeDiameter.h>

/* Internal to the library */
extern const char * type_base_name[];
void fd_msg_eteid_init(void);
int fd_sess_init(void);
void fd_sess_fini(void);
int fd_sig_init(void);
void fd_sig_fini(void);

/* Iterator on the rules of a parent object */
int fd_dict_iterate_rules ( struct dict_object *parent, void * data, int (*cb)(void *, struct dict_rule_data *) );

/* Dispatch / messages / dictionary API */
int fd_dict_disp_cb(enum dict_object_type type, struct dict_object *obj, struct fd_list ** cb_list);
void fd_dict_dump_avp_value(union avp_value *avp_value, struct dict_object * model, int indent);
int fd_disp_call_cb_int( struct fd_list * cb_list, struct msg ** msg, struct avp *avp, struct session *sess, enum disp_action *action, 
			struct dict_object * obj_app, struct dict_object * obj_cmd, struct dict_object * obj_avp, struct dict_object * obj_enu);
extern pthread_rwlock_t fd_disp_lock;

/* Messages / sessions API */
int fd_sess_fromsid_msg ( unsigned char * sid, size_t len, struct session ** session, int * new);
int fd_sess_reclaim_msg ( struct session ** session );

#endif /* _LIBFD_H */
