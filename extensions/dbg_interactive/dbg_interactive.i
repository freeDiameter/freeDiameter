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



/***********************************
 Some types & typemaps for usability 
 ***********************************/

/* for fd_hash */
%apply (char *STRING, size_t LENGTH) { ( char * string, size_t len ) };

/* for dictionary functions */
%typemap(in, numinputs=0,noblock=1) SWIGTYPE ** OUTPUT (void *temp = NULL) {
	$1 = (void *)&temp;
}
%typemap(argout,noblock=1) SWIGTYPE ** OUTPUT {
	%append_output(SWIG_NewPointerObj(*$1, $*1_descriptor, 0));
}
%apply SWIGTYPE ** OUTPUT { struct dict_object ** ref };
%apply SWIGTYPE ** OUTPUT { struct dict_object ** result };



/*********************************************************
 Now, create wrappers for (almost) all objects from fD API 
 *********************************************************/
%include "freeDiameter/freeDiameter-host.h"
%include "freeDiameter/libfreeDiameter.h"
%include "freeDiameter/freeDiameter.h"



/**********************************************************/
/* The remaining of this file allows easier manipulation of
the structures and functions of fD by providing wrapper-specific
extensions to the freeDiameter API.

/****** LISTS *********/

%extend fd_list {
	/* allow a parameter in the constructor, and perform the fd_list_init operation */
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
	/* Unlink before freeing */
	~fd_list() {
		fd_list_unlink($self);
		free($self);
	}
	/* For debug, show the values of the list */
	void dump() {
		fd_log_debug("list: %p\n", $self);
		fd_log_debug("  - next: %p\n", $self->next);
		fd_log_debug("  - prev: %p\n", $self->prev);
		fd_log_debug("  - head: %p\n", $self->head);
		fd_log_debug("  - o   : %p\n", $self->o);
	}
};

/****** DICTIONARY *********/

struct dictionary {
};

%extend dictionary {
	dictionary() {
		struct dictionary * r = NULL;
		int ret = fd_dict_init(&r);
		if (ret != 0) {
			fd_log_debug("Error: %s\n", strerror(ret));
			PyErr_SetString(PyExc_MemoryError,"Not enough memory");
			return NULL;
		}
		return r;
	}
	~dictionary() {
		if (self) {
			struct dictionary *d = self;
			int ret = fd_dict_fini(&d);
			if (ret != 0) {
				fd_log_debug("Error: %s\n", strerror(ret));
			}
			return;
		}
	}
	void dump() {
		if ($self) {
			fd_dict_dump($self);
		}
	}
	PyObject * vendors_list() {
		uint32_t *list = NULL, *li;
		PyObject * ret;
		if (!$self) {
			PyErr_SetString(PyExc_SyntaxError,"dict_object cannot be created directly. Use fd_dict_new.");
			return NULL;
		}
		ret = PyList_New(0);
		list = fd_dict_get_vendorid_list($self);
		for (li = list; *li != 0; li++) {
			PyList_Append(ret, PyInt_FromLong((long)*li));
		}
		free(list);
		return ret;
	}
	
}

struct dict_object {
};

%extend dict_object {
	dict_object() {
		fd_log_debug("Error: dict_object cannot be created directly. Use fd_dict_new\n");
		PyErr_SetString(PyExc_SyntaxError,"dict_object cannot be created directly. Use fd_dict_new.");
		return NULL;
	}
	~dict_object() {
		fd_log_debug("Error: dict_object cannot be destroyed directly. Destroy the parent dictionary.\n");
		return;
	}
	void dump() {
		if ($self) {
			fd_dict_dump_object($self);
		}
	}
}

/* overload the search function to allow passing integers & string criteria directly */
%rename(fd_dict_search) fd_dict_search_int;
%inline %{
int fd_dict_search_int ( struct dictionary * dict, enum dict_object_type type, int criteria, int what_by_val, struct dict_object ** result, int retval ) {
	return fd_dict_search ( dict, type, criteria, &what_by_val, result, retval );
}
%}
%rename(fd_dict_search) fd_dict_search_string;
%inline %{
int fd_dict_search_string ( struct dictionary * dict, enum dict_object_type type, int criteria, char * what_by_string, struct dict_object ** result, int retval ) {
	return fd_dict_search ( dict, type, criteria, what_by_string, result, retval );
}
%}


%extend avp_value_os {
	~avp_value_os() {
		if (self)
			free(self->data);
	}
	void dump() {
		if ($self) {
			%#define LEN_MAX 20
			int i, n=LEN_MAX;
			if ($self->len < LEN_MAX)
				n = $self->len;
			fd_log_debug("l:%u, v:[", $self->len);
			for (i=0; i < n; i++)
				fd_log_debug("%02.2X", $self->data[i]);
			fd_log_debug("] '%.*s%s'\n", n, $self->data, n == LEN_MAX ? "..." : "");
		}
	}
}

%extend avp_value {
	void os_set(char *STRING, size_t LENGTH) {
		free($self->os.data);
		$self->os.data = malloc(LENGTH);
		if (!$self->os.data) {
			fd_log_debug("Out of memory!\n");
			PyErr_SetString(PyExc_MemoryError,"Not enough memory");
			return;
		}
		memcpy($self->os.data, STRING, LENGTH);
		$self->os.len = LENGTH;
	}
	void os_set(avp_value_os * os) {
		free($self->os.data);
		$self->os.data = malloc(os->len);
		if (!$self->os.data) {
			fd_log_debug("Out of memory!\n");
			PyErr_SetString(PyExc_MemoryError,"Not enough memory");
			return;
		}
		memcpy($self->os.data, os->data, os->len);
		$self->os.len = os->len;
	}
};

%cstring_output_allocate_size(char ** swig_buffer, size_t * swig_len, free(*$1))
%inline %{
int fd_msg_bufferize_py ( struct msg * msg, char ** swig_buffer, size_t * swig_len ) {
	return fd_msg_bufferize(msg, (void *)swig_buffer, swig_len);
}
%};

