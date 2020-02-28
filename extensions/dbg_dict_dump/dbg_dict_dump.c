/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Luke Mewburn <luke@mewburn.net>								 *
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
 * Dump Diameter dictionary (using fd_dict_dump()) to a file or the log.
 *
 * If conffile is provided, write the dump to that file,
 * otherwise write to log.
 */

#include <freeDiameter/extension.h>

static int dbg_dict_dump_entry(char * conffile)
{
	TRACE_ENTRY("%p", conffile);

	FILE * out = NULL;
	if (conffile) {
		LOG_N("Dictionary dump to file '%s'", conffile);
		out = fopen(conffile, "w");
		if (NULL == out) {
			LOG_E("Cannot open output file '%s' for writing", conffile);
			return EINVAL;
		}
	} else {
		LOG_N("Dictionary dump to log");
	}

	char * tbuf = NULL; size_t tbuflen = 0;
	if (NULL == fd_dict_dump(&tbuf, &tbuflen, NULL, fd_g_config->cnf_dict)) {
		LOG_E("Cannot dump dictionary");
	} else if (out) {
		fprintf(out, "%s\n", tbuf);
	} else {
		LOG_N("%s", fd_dict_dump(&tbuf, &tbuflen, NULL, fd_g_config->cnf_dict));
	}
	free(tbuf);
	if (out) {
		fclose(out);
		out = NULL;
	}

	LOG_N("Dictionary dump completed");
	return 0;
}

void fd_ext_fini(void)
{
	TRACE_ENTRY();

	/* Nothing to do */
	return;
}

EXTENSION_ENTRY("dbg_dict_dump", dbg_dict_dump_entry);
