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

/* Do not include this directly, use dbg_interactive.i instead */

/****** ROUTING *********/

struct rt_data {
};

%extend rt_data {
	rt_data() {
		struct rt_data * r = NULL;
		int ret = fd_rtd_init(&r);
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
			return NULL;
		}
		return r;
	}
	~rt_data() {
		struct rt_data *r = self;
		fd_rtd_free(&r);
	}
	void add(char * peerid, char * realm) {
		int ret = fd_rtd_candidate_add($self, peerid, realm);
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
		}
	}
	void remove(char * STRING, size_t LENGTH) {
		fd_rtd_candidate_del($self, STRING, LENGTH);
	}
	void error(char * dest, char * STRING, size_t LENGTH, uint32_t rcode) {
		int ret =  fd_rtd_error_add($self, dest, (uint8_t *)STRING, LENGTH, rcode);
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
		}
	}
	struct fd_list * extract(int score = 0) {
		struct fd_list * li = NULL;
		fd_rtd_candidate_extract($self, &li, score);
		return li;
	}
}

%extend rtd_candidate {
	void dump() {
		fd_log_debug("candidate %p\n", $self);
		fd_log_debug("  id : %s\n",  $self->diamid);
		fd_log_debug("  rlm: %s\n", $self->realm);
		fd_log_debug("  sc : %d\n", $self->score);
	}
}

