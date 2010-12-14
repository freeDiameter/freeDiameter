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

/* Some functions are not available through the wrapper, or accessed differently */
%ignore fd_lib_init;
%ignore fd_lib_fini;
%ignore fd_dict_init;
%ignore fd_dict_fini;
%ignore fd_sess_handler_create_internal;
%ignore fd_sess_handler_destroy;
%ignore fd_sess_new;
%ignore fd_sess_getsid;
%ignore fd_sess_destroy;
%ignore fd_sess_reclaim;
%ignore fd_sess_state_store_internal;
%ignore fd_sess_state_retrieve_internal;


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
%apply SWIGTYPE ** OUTPUT { struct dict_object ** obj };
%apply SWIGTYPE ** OUTPUT { struct dict_object ** result };
%apply SWIGTYPE ** OUTPUT { struct dictionary  ** dict }; /* this is for fd_dict_getdict, not fd_dict_init (use constructor) */
%apply int * OUTPUT { enum dict_object_type * type };
%apply (char *STRING, size_t LENGTH) { (char * sid, size_t len) };
%apply SWIGTYPE ** OUTPUT { struct session ** session };
%apply int * OUTPUT { int * new };

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

/* The following wrapper leaks memory each time an union avp_value is assigned an octet string.
 TODO: fix this leak by better understanding SWIG... 
   -- the alternative is to uncomment the "free" statements bellow, but then it is easy to
   create a segmentation fault by assigning first an integer, then an octetstring.
 */
%extend avp_value {
	/* The following hack in the proxy file allows assigning the octet string directly like this:
	avp_value.os = "blabla"
	*/
	%pythoncode
	{
    __swig_setmethods__["os"] = _fDpy.avp_value_os_set
    if _newclass:os = _swig_property(_fDpy.avp_value_os_get, _fDpy.avp_value_os_set)
	}
	void os_set(char *STRING, size_t LENGTH) {
		/* free($self->os.data);  -- do not free, in case the previous value was not an OS */
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
		/* free($self->os.data);  -- do not free, in case the previous value was not an OS */
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

%extend avp_value_os {
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

/****** SESSIONS *********/

%{
/* At the moment, only 1 callback is supported... */
static PyObject * py_cleanup_cb = NULL;
static void call_the_python_cleanup_callback(session_state * state, char * sid) {
	PyObject *result;
	if (!py_cleanup_cb)
		return;
	
	/* Call the function */
	result = PyEval_CallFunction(py_cleanup_cb, "(Os)", state, sid);
	
	Py_XDECREF(result);
	return;
}
%}

struct session_handler {
};

%extend session_handler {
	session_handler() {
		fd_log_debug("Error: a cleanup callback parameter is required.\n");
		PyErr_SetString(PyExc_SyntaxError,"Error: a cleanup callback parameter is required.\n");
		return NULL;
	}
	session_handler(PyObject * PyCleanupCb) {
		struct session_handler * hdl = NULL;
		int ret;
		if (py_cleanup_cb) {
			fd_log_debug("dbg_interactive supports only 1 session handler in python at the moment\n");
			PyErr_SetString(PyExc_SyntaxError,"dbg_interactive supports only 1 session handler in python at the moment\n");
			return NULL;
		}
		if (!PyCallable_Check(PyCleanupCb)) {
			PyErr_SetString(PyExc_TypeError, "Need a callable object!");
			return NULL;
		}
		py_cleanup_cb = PyCleanupCb;
		Py_XINCREF(py_cleanup_cb);
		
		ret = fd_sess_handler_create_internal ( &hdl, call_the_python_cleanup_callback );
		if (ret != 0) {
			fd_log_debug("Error: %s\n", strerror(ret));
			PyErr_SetString(PyExc_MemoryError,"Not enough memory");
			return NULL;
		}
		return hdl;
	}
	~session_handler() {
		if (self) {
			struct session_handler * hdl = self;
			int ret = fd_sess_handler_destroy(&hdl);
			if (ret != 0) {
				fd_log_debug("Error: %s\n", strerror(ret));
			}
			return;
		}
	}
	void dump() {
		if ($self) {
			fd_sess_dump_hdl(0, $self);
		}
	}
}

struct session {
};

%extend session {
	/* The first two versions create a new session string. The third one allow to use an existing string. */
	session() {
		int ret;
		struct session * s = NULL;
		ret = fd_sess_new(&s, fd_g_config->cnf_diamid, "dbg_interactive", sizeof("dbg_interactive"));
		if (ret != 0) {
			fd_log_debug("Error: %s\n", strerror(ret));
			PyErr_SetString(PyExc_MemoryError,"Not enough memory");
			return NULL;
		}
		return s;
	}
	session(char * diamid, char * STRING, size_t LENGTH) {
		int ret;
		struct session * s = NULL;
		ret = fd_sess_new(&s, diamid, STRING, LENGTH);
		if (ret != 0) {
			fd_log_debug("Error: %s\n", strerror(ret));
			PyErr_SetString(PyExc_MemoryError,"Not enough memory");
			return NULL;
		}
		return s;
	}
	session(char * STRING, size_t LENGTH) {
		int ret, n;
		struct session * s = NULL;
		ret = fd_sess_fromsid(STRING, LENGTH, &s, &n);
		if (ret != 0) {
			fd_log_debug("Error: %s\n", strerror(ret));
			PyErr_SetString(PyExc_MemoryError,"Not enough memory");
			return NULL;
		}
		/* When defining n as OUTPUT parameter, we get something strange... Use fd_sess_fromsid if you need it */
		if (n) {
			fd_log_debug("A new session has been created\n");
		} else {
			fd_log_debug("A session with same id already existed\n");
		}
		return s;
	}
	~session() {
		if (self) {
			struct session * s = self;
			int ret = fd_sess_reclaim(&s);
			if (ret != 0) {
				fd_log_debug("Error: %s\n", strerror(ret));
			}
			return;
		}
	}
	char * getsid() {
		int ret;
		char * sid = NULL;
		if (!$self)
			return NULL;
		ret = fd_sess_getsid( $self, &sid);
		if (ret != 0) {
			fd_log_debug("Error: %s\n", strerror(ret));
			PyErr_SetString(PyExc_MemoryError,"Problem...");
			return NULL;
		}
		return sid;
	}
	void settimeout(long seconds) {
		struct timespec timeout;
		int ret;
		clock_gettime(CLOCK_REALTIME, &timeout);
		timeout.tv_sec += seconds;
		ret = fd_sess_settimeout( $self, &timeout );
		if (ret != 0) {
			fd_log_debug("Error: %s\n", strerror(ret));
			PyErr_SetString(PyExc_MemoryError,"Problem...");
		}
	}
	void dump() {
		if ($self) {
			fd_sess_dump(0, $self);
		}
	}
	void store(struct session_handler * handler, PyObject * state) {
		int ret;
		void * store = state;
		Py_INCREF(state);
		ret = fd_sess_state_store_internal(handler, $self, (void *) &store);
		if (ret != 0) {
			fd_log_debug("Error: %s\n", strerror(ret));
			PyErr_SetString(PyExc_MemoryError,"Problem...");
		}
	}
	PyObject *  retrieve(struct session_handler * handler) {
		int ret;
		PyObject * state = NULL;
		ret = fd_sess_state_retrieve_internal(handler, $self, (void *) &state);
		if (ret != 0) {
			fd_log_debug("Error: %s\n", strerror(ret));
			PyErr_SetString(PyExc_MemoryError,"Problem...");
		}
		if (state == NULL) {
			return Py_None;
		}
		Py_DECREF(state);
		return state;
	}
}	



/****** MESSAGES *********/


%cstring_output_allocate_size(char ** swig_buffer, size_t * swig_len, free(*$1))
%inline %{
int fd_msg_bufferize_py ( struct msg * msg, char ** swig_buffer, size_t * swig_len ) {
	return fd_msg_bufferize(msg, (void *)swig_buffer, swig_len);
}
%};

