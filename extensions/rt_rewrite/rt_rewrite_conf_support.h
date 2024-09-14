/**********************************************************************************************************
 * Software License Agreement (BSD License)                                                               *
 * Author: Thomas Klausner <tk@giga.or.at>                                                                *
 *                                                                                                        *
 * Copyright (c) 2018, 2023 Thomas Klausner                                                               *
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

#include <stdio.h>

#include "rt_rewrite.h"

int add_finish(const char *value);
void avp_add_free(struct avp_add *item);
void avp_match_free(struct avp_match *match);
struct avp_match *avp_match_new(const char *name);
int add_request(const char *request_name);
void compare_avp_types(struct avp_match *start);
int condition(const char *name, enum comparators comparator, const char *value);
int dest_add(const char *name);
int drop_finish(void);
void dump_add_config(struct avp_add *start);
void dump_config(struct avp_match *start, const char *prefix);
int map_finish(void);
void rt_rewrite_confrestart(FILE *input_file);
int source_add(const char *name);
int variable_add(const char *name);
int variable_finish(void);
void variable_names_cleanup(void);
int variable_set_name(const char *name);
