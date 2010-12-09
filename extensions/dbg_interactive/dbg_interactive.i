/* This interface file is processed by SWIG to create a python wrapper interface to freeDiameter framework. */
%module fDpy
%begin %{
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
%}

%{
/* This text is included in the generated wrapper verbatim */
#define SWIG
#include <freeDiameter/extension.h>
%}


/* Include standard types & functions used in freeDiameter headers */
%include <stdint.i>
%include <cpointer.i>
%include <cdata.i>
%include <cstring.i>
%include <typemaps.i>

/* Some functions are not available through the wrapper */
%ignore fd_lib_init;
%ignore fd_lib_fini;

/* Inline functions seems to give problems to SWIG -- just remove the inline definition */
%define __inline__ 
%enddef

/* Make some global-variables read-only (mainly to avoid warnings) */
%immutable fd_g_config;
%immutable peer_state_str;

/* Overwrite a few functions prototypes for usability: default parameters values, OUTPUT typemaps, ... */
extern void fd_list_init ( struct fd_list * list, void * obj = NULL );


/*
extern int fd_dict_new ( struct dictionary * dict, enum dict_object_type type, void * data, struct dict_object * parent, struct dict_object ** OUTPUT );
extern int fd_dict_search ( struct dictionary * dict, enum dict_object_type type, int criteria, void * what, struct dict_object ** OUTPUT, int retval );
extern int fd_dict_get_error_cmd(struct dictionary * dict, struct dict_object ** OUTPUT);
extern int fd_dict_getval ( struct dict_object * object, void * INOUT);
//extern int fd_dict_gettype ( struct dict_object * object, enum dict_object_type * OUTPUT);
extern int fd_dict_getdict ( struct dict_object * object, struct dictionary ** OUTPUT);
*/

/* Retrieve the compile-time definitions of freeDiameter */
%include "freeDiameter/freeDiameter-host.h"
%include "freeDiameter/libfreeDiameter.h"
%include "freeDiameter/freeDiameter.h"


/* Some pointer types that are useful */
%pointer_functions(int, int_ptr);
%pointer_functions(uint8_t *, uint8_t_pptr);
%pointer_cast(char *, void *, char_to_void);

%pointer_functions(struct fd_list *, fd_list_pptr);

%pointer_functions(enum dict_object_type, dict_object_type_ptr);
%pointer_functions(struct dict_object *, dict_object_pptr);

%pointer_functions(struct session *, session_pptr);
%pointer_functions(struct session_handler *, session_handler_pptr);
%pointer_functions(session_state *, session_state_pptr);

%pointer_functions(struct rt_data *, rt_data_pptr);
%pointer_cast(struct fd_list *, struct rtd_candidate *, fd_list_to_rtd_candidate);

%pointer_functions(struct msg *, msg_pptr);
%pointer_functions(struct msg_hdr *, msg_hdr_pptr);
%pointer_functions(struct avp *, avp_pptr);
%pointer_functions(struct avp_hdr *, avp_hdr_pptr);


/* Extend some structures for usability/debug in python */
%extend fd_list {
	fd_list(void * o = NULL) {
		struct fd_list * li;
		li = (struct fd_list *) malloc(sizeof(struct fd_list));
		if (!li) {
			fd_log_debug("Out of memory!\n");
			PyErr_SetString(PyExc_MemoryError,"Not enough memory");
			return NULL;
		}
		fd_list_init(li, o);
		return li;
	}
	~fd_list() {
		fd_list_unlink($self);
		free($self);
	}
	void dump() {
		fd_log_debug("list: %p\n", $self);
		fd_log_debug("  - next: %p\n", $self->next);
		fd_log_debug("  - prev: %p\n", $self->prev);
		fd_log_debug("  - head: %p\n", $self->head);
		fd_log_debug("  - o   : %p\n", $self->o);
	}
};

%inline %{
void dict_object_type_ptr_dump(enum dict_object_type * t) {
	#define CASE_STR(x)  case x: fd_log_debug(#x "\n"); break;
	switch (*t) {
		CASE_STR(DICT_VENDOR)
		CASE_STR(DICT_APPLICATION)
		CASE_STR(DICT_TYPE)
		CASE_STR(DICT_ENUMVAL)
		CASE_STR(DICT_AVP)
		CASE_STR(DICT_COMMAND)
		CASE_STR(DICT_RULE)
		default: 
			fd_log_debug("Invalid value (%d)", *t); 
			PyErr_SetString(PyExc_IndexError,"Invalid dictionary type object");
	}
}
%}

%extend avp_value_os {
	void fromstr(char * string) {
		if ($self->data) free($self->data);
		$self->data = (uint8_t *)strdup(string);
		if (!$self->data) {
			fd_log_debug("Out of memory!\n");
			PyErr_SetString(PyExc_MemoryError,"Not enough memory");
			$self->len = 0;
			return;
		}
		$self->len = strlen(string);
	}
	%newobject as_str;
	char * tostr() {
		char * r;
		if ($self->len == 0)
			return NULL;
		r = malloc($self->len + 1);
		if (r) {
			strncpy(r, (char *)$self->data, $self->len + 1);
		} else {
			fd_log_debug("Out of memory!\n");
			PyErr_SetString(PyExc_MemoryError,"Not enough memory");
		}
		return r;
	}
};	

%extend avp_value {
	void os_set(void * os) {
		memcpy(&$self->os, os, sizeof($self->os));
	}
};	

%cstring_output_allocate_size(char ** swig_buffer, size_t * swig_len, free(*$1))
%inline %{
int fd_msg_bufferize_py ( struct msg * msg, char ** swig_buffer, size_t * swig_len ) {
	return fd_msg_bufferize(msg, (void *)swig_buffer, swig_len);
}
%};

