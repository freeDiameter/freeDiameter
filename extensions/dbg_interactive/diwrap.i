/* This interface file is processed by SWIG to create a python wrapper interface to freeDiameter framework. */
%module diwrap
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


%init %{
/* TODO: How to load the proxy classes here? */
%}

%{
/* Define types etc. */
#define SWIG
#include <freeDiameter/extension.h>
%}


/* Include standard types & functions used in freeDiameter headers */
%include <stdint.i>
%include <cpointer.i>
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
%pointer_class(int, int_ptr);
%pointer_class(enum dict_object_type, dict_object_type_ptr);
%pointer_functions(struct dict_object *, dict_object_ptr);
%pointer_functions(struct session *, session_ptr);




/* Extend some structures for usability/debug in python */
%extend fd_list {
	fd_list(void * o = NULL) {
		struct fd_list * li;
		li = (struct fd_list *) malloc(sizeof(struct fd_list));
		if (!li) {
			fd_log_debug("Out of memory!\n");
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

%extend dict_object_type_ptr {
	void dump() {
		%#define CASE_STR(x)  case x: fd_log_debug(#x "\n"); break;
		switch (*$self) {
			CASE_STR(DICT_VENDOR)
			CASE_STR(DICT_APPLICATION)
			CASE_STR(DICT_TYPE)
			CASE_STR(DICT_ENUMVAL)
			CASE_STR(DICT_AVP)
			CASE_STR(DICT_COMMAND)
			CASE_STR(DICT_RULE)
			default: fd_log_debug("Invalid value (%d)", *$self); break;
		}
	}
}

%inline %{ 
void session_ptr_showsid(struct session * s) {
	char * sid;
	int ret = fd_sess_getsid ( s, &sid );
	if (ret != 0) {
		fd_log_debug("Error %d\n", ret);
		/* throw an exception in SWIG? */
		return;
	}
	fd_log_debug("%s\n", sid);
}
%}
