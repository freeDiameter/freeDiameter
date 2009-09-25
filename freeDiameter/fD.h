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

/* This file contains the definitions for internal use in the freeDiameter daemon */

#ifndef _FD_H
#define _FD_H

#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/freeDiameter.h>

/* Configuration */
int fd_conf_init();
void fd_conf_dump();
int fd_conf_parse();
int fddparse(struct fd_config * conf); /* yacc generated */

/* Extensions */
int fd_ext_init();
int fd_ext_add( char * filename, char * conffile );
int fd_ext_load();
void fd_ext_dump(void);
int fd_ext_fini(void);

/* Messages */
int fd_msg_init(void);

/* Global message queues */
extern struct fifo * fd_g_incoming; /* all messages received from other peers, except local messages (CER, ...) */
extern struct fifo * fd_g_outgoing; /* messages to be sent to other peers on the network following routing procedure */
extern struct fifo * fd_g_local; /* messages to be handled to local extensions */
/* Message queues */
int fd_queues_init(void);
int fd_queues_fini(void);

/* Create all the dictionary objects defined in the Diameter base RFC. */
int fd_dict_base_protocol(struct dictionary * dict);

#endif /* _FD_H */
