/*********************************************************************************************************
 * Software License Agreement (BSD License)                                                               *
 * Author: Thomas Klausner <tk@giga.or.at>                                                                *
 *                                                                                                        *
 * Copyright (c) 2018, 2023 Thomas Klausner                                                                    *
 * All rights reserved.                                                                                   *
 *                                                                                                        *
 * Written under contract by Effortel Technologies SA, http://effortel.com/                               *
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

/* FreeDiameter's common include file */
#include <freeDiameter/extension.h>

/* Parse the configuration file */
int rt_rewrite_conf_handle(char * conffile);

typedef enum { REWRITE_DROP, REWRITE_MAP } avp_rewrite_type;

/* start of the list of rt_rewrite entries */

extern struct avp_match *avp_match_start;
extern struct avp_add *avp_add_start;

/*
 * The avp_match structure is used to build up a tree.
 * 'name' contains the name of the current node, or "top" for the root of the tree.
 * 'next' contains a link to the next node on the same level
 * 'children' contains a link to the next node on the lower level
 *
 * The other data is for keeping information for ADD/DROP/MAP.
 * 'match_type' determines which of the three types this is.
 * MAP:
 * 'target' determines where to put the value of this AVP
 */

struct avp_match {
	char *name;

	struct avp_match *next;
	struct avp_match *children;

	avp_rewrite_type match_type;
	/* for MAP */
        struct avp_target *target;
};

/*
 * The avp_target structure is used for keeping a path to an AVP through grouped AVPs.
 * 'name' contains the name of the AVP, and
 * 'child' is a link to the next AVP on the level below, or NULL.
 */
struct avp_target {
	char *name;

	struct avp_target *child;
};

/*
 * 'avp_add_list' is a list of entries to be added to messages.
 *
 * 'request_type' is the type of messages to which the AVP should be added.
 * 'target' is the path to the AVP to be added.
 * 'value' is the value to be set for this AVP.
 *
 * 'next' points to the next member of the list, if any.
 */
struct avp_add {
	struct dict_cmd_data request;

	struct avp_target *target;
	union avp_value value;
	int free_os_data;

	struct avp_add *next;
};

void avp_match_free(struct avp_match *match);
void avp_add_free(struct avp_add *item);
