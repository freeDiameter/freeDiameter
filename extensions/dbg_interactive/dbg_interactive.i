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
/* -- the following functions are better accessed differently, but we leave their definitions just in case
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
%ignore fd_rtd_init;
%ignore fd_rtd_free;
%ignore fd_rtd_candidate_add;
%ignore fd_rtd_candidate_del;
%ignore fd_rtd_candidate_extract;
%ignore fd_rtd_error_add;
*/

/* Inline functions seems to give problems to SWIG -- just remove the inline definition */
%define __inline__ 
%enddef

/* Make some global-variables read-only (mainly to avoid warnings) */
%immutable fd_g_config;
%immutable peer_state_str;


/* Create a generic error handling mechanism so that functions can provoke an exception */
%{
/* This is not thread-safe etc. but it should work /most of the time/. */
static int wrapper_errno;
static PyObject* wrapper_errno_py;
static char * wrapper_error_txt; /* if NULL, use strerror(errno) */
#define DI_ERROR(code, pycode, str) {	\
	fd_log_debug("[dbg_interactive] ERROR: %s: %s\n", __PRETTY_FUNCTION__, str ? str : strerror(code)); \
	wrapper_errno = code;		\
	wrapper_errno_py = pycode;	\
	wrapper_error_txt = str;	\
}

#define DI_ERROR_MALLOC	\
	 DI_ERROR(ENOMEM, PyExc_MemoryError, NULL)

%}

%exception {
	/* reset the errno */
	wrapper_errno = 0;
	/* Call the function  -- it will use DI_ERROR macro in case of error */
	$action
	/* Now, test for error */
	if (wrapper_errno) {
		char * str = wrapper_error_txt ? wrapper_error_txt : strerror(wrapper_errno);
		PyObject * exc = wrapper_errno_py;
		if (!exc) {
			switch (wrapper_errno) {
				case ENOMEM: exc = PyExc_MemoryError; break;
				case EINVAL: exc = PyExc_ValueError; break;
				default: exc = PyExc_RuntimeError;
			}
		}
		SWIG_PYTHON_THREAD_BEGIN_BLOCK;
		PyErr_SetString(exc, str);
		SWIG_fail;
		SWIG_PYTHON_THREAD_END_BLOCK;
	}
}


/***********************************
 Some types & typemaps for usability 
 ***********************************/

%apply (char *STRING, size_t LENGTH) { ( char * string, size_t len ) };

%typemap(in, numinputs=0,noblock=1) SWIGTYPE ** OUTPUT (void *temp = NULL) {
	$1 = (void *)&temp;
}
%typemap(argout,noblock=1) SWIGTYPE ** OUTPUT {
	%append_output(SWIG_NewPointerObj(*$1, $*1_descriptor, 0));
}

%apply int * OUTPUT { enum dict_object_type * type };
%apply (char *STRING, size_t LENGTH) { (char * sid, size_t len) };
%apply SWIGTYPE ** OUTPUT { struct session ** session };
%apply int * OUTPUT { int * new };

/* Callbacks defined in python */
%typemap(in) PyObject *PyCb {
	if (!PyCallable_Check($input)) {
		PyErr_SetString(PyExc_TypeError, "Need a callable object!");
		SWIG_fail;
	}
	$1 = $input;
}



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
			DI_ERROR_MALLOC;
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
			DI_ERROR(ret, NULL, NULL);
			return NULL;
		}
		return r;
	}
	~dictionary() {
		struct dictionary *d = self;
		int ret = fd_dict_fini(&d);
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
		}
		return;
	}
	void dump() {
		fd_dict_dump($self);
	}
	PyObject * vendors_list() {
		uint32_t *list = NULL, *li;
		PyObject * ret;
		SWIG_PYTHON_THREAD_BEGIN_BLOCK;
		ret = PyList_New(0);
		list = fd_dict_get_vendorid_list($self);
		for (li = list; *li != 0; li++) {
			PyList_Append(ret, PyInt_FromLong((long)*li));
		}
		free(list);
		SWIG_PYTHON_THREAD_END_BLOCK;
		return ret;
	}
	struct dict_object * new_obj(enum dict_object_type type, void * data, struct dict_object * parent = NULL) {
		struct dict_object * obj = NULL;
		int ret = fd_dict_new($self, type, data, parent, &obj);
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
			return NULL;
		}
		return obj;
	}
	struct dict_object * search(enum dict_object_type type, int criteria, int what_by_val) {
		struct dict_object * obj = NULL;
		int ret = fd_dict_search ( $self, type, criteria, &what_by_val, &obj, ENOENT );
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
			return NULL;
		}
		return obj;
	}
	struct dict_object * search(enum dict_object_type type, int criteria, char * what_by_string) {
		struct dict_object * obj = NULL;
		int ret = fd_dict_search ( $self, type, criteria, what_by_string, &obj, ENOENT );
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
			return NULL;
		}
		return obj;
	}
	struct dict_object * search(enum dict_object_type type, int criteria, void * what) {
		struct dict_object * obj = NULL;
		int ret = fd_dict_search ( $self, type, criteria, what, &obj, ENOENT );
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
			return NULL;
		}
		return obj;
	}
	struct dict_object * error_cmd() {
		struct dict_object * obj = NULL;
		int ret = fd_dict_get_error_cmd ( $self, &obj );
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
			return NULL;
		}
		return obj;
	}
}

struct dict_object {
};

%extend dict_object {
	dict_object() {
		DI_ERROR(EINVAL, PyExc_SyntaxError, "dict_object cannot be created directly. Use fd_dict_new().");
		return NULL;
	}
	~dict_object() {
		DI_ERROR(EINVAL, PyExc_SyntaxError, "dict_object cannot be destroyed directly. Destroy the parent dictionary.");
		return;
	}
	void dump() {
		fd_dict_dump_object($self);
	}
	enum dict_object_type gettype() {
		enum dict_object_type t;
		int ret = fd_dict_gettype ( $self, &t);
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
			return 0;
		}
		return t;
	}
	struct dictionary * getdict() {
		struct dictionary *d;
		int ret = fd_dict_getdict ( $self, &d );
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
			return NULL;
		}
		return d;
	}
	/* Since casting the pointer requires intelligence, we do it here instead of giving it to SWIG */
	PyObject * getval() {
		/* first, get the type */
		enum dict_object_type t;
		int ret = fd_dict_gettype ( $self, &t);
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
			return NULL;
		}
		switch (t) {
%define %GETVAL_CASE(TYPE,STRUCT)
			case TYPE: {
				PyObject * v = NULL;
				struct STRUCT * data = NULL;
				data = malloc(sizeof(struct STRUCT));
				if (!data) {
					DI_ERROR_MALLOC;
					return NULL;
				}
				ret = fd_dict_getval($self, data);
				if (ret != 0) {
					DI_ERROR(ret, NULL, NULL);
					free(data);
					return NULL;
				}
				SWIG_PYTHON_THREAD_BEGIN_BLOCK;
				v = SWIG_NewPointerObj((void *)data, SWIGTYPE_p_##STRUCT, SWIG_POINTER_OWN );
				Py_XINCREF(v);
				SWIG_PYTHON_THREAD_END_BLOCK;
				return v;
			} break
%enddef
			%GETVAL_CASE( DICT_VENDOR, 	dict_vendor_data );
			%GETVAL_CASE( DICT_APPLICATION, dict_application_data );
			%GETVAL_CASE( DICT_TYPE, 	dict_type_data );
			%GETVAL_CASE( DICT_ENUMVAL, 	dict_enumval_data );
			%GETVAL_CASE( DICT_AVP, 	dict_avp_data );
			%GETVAL_CASE( DICT_COMMAND, 	dict_cmd_data );
			%GETVAL_CASE( DICT_RULE, 	dict_rule_data );
			default:
				DI_ERROR(EINVAL, PyExc_SystemError, "Internal error: Got invalid object type");
		}
		return NULL;
	}
}


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
			DI_ERROR_MALLOC;
			return;
		}
		memcpy($self->os.data, STRING, LENGTH);
		$self->os.len = LENGTH;
	}
	void os_set(avp_value_os * os) {
		/* free($self->os.data);  -- do not free, in case the previous value was not an OS */
		$self->os.data = malloc(os->len);
		if (!$self->os.data) {
			DI_ERROR_MALLOC;
			return;
		}
		memcpy($self->os.data, os->data, os->len);
		$self->os.len = os->len;
	}
};

%extend avp_value_os {
	void dump() {
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

/****** SESSIONS *********/

%{
/* store the python callback function here */
static PyObject * py_cleanup_cb = NULL;
/* call it (might be called from a different thread than the interpreter, when session times out) */
static void call_the_python_cleanup_callback(session_state * state, char * sid) {
	PyObject *result;
	if (!py_cleanup_cb)
		return;
	
	/* Call the function */
	SWIG_PYTHON_THREAD_BEGIN_BLOCK;
	result = PyEval_CallFunction(py_cleanup_cb, "(Os)", state, sid);
	Py_XDECREF(result);
	SWIG_PYTHON_THREAD_END_BLOCK;
	return;
}
%}

struct session_handler {
};

%extend session_handler {
	session_handler() {
		DI_ERROR(EINVAL, PyExc_SyntaxError, "a cleanup callback parameter is required.");
		return NULL;
	}
	session_handler(PyObject * PyCb) {
		struct session_handler * hdl = NULL;
		int ret;
		if (py_cleanup_cb) {
			DI_ERROR(EINVAL, PyExc_SyntaxError, "Only one session handler at a time is supported at the moment in this extension\n.");
			return NULL;
		}
		py_cleanup_cb = PyCb;
		Py_XINCREF(py_cleanup_cb);
		
		ret = fd_sess_handler_create_internal ( &hdl, call_the_python_cleanup_callback );
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
			return NULL;
		}
		return hdl;
	}
	~session_handler() {
		struct session_handler * hdl = self;
		int ret = fd_sess_handler_destroy(&hdl);
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
		}
		/* Now free the callback */
		Py_XDECREF(py_cleanup_cb);
		py_cleanup_cb = NULL;
		return;
	}
	void dump() {
		fd_sess_dump_hdl(0, $self);
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
			DI_ERROR(ret, NULL, NULL);
			return NULL;
		}
		return s;
	}
	session(char * diamid, char * STRING, size_t LENGTH) {
		int ret;
		struct session * s = NULL;
		ret = fd_sess_new(&s, diamid, STRING, LENGTH);
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
			return NULL;
		}
		return s;
	}
	session(char * STRING, size_t LENGTH) {
		int ret, n;
		struct session * s = NULL;
		ret = fd_sess_fromsid(STRING, LENGTH, &s, &n);
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
			return NULL;
		}
		/* When defining n as OUTPUT parameter, we get something strange... Use fd_sess_fromsid if you need it */
		#if 0
		if (n) {
			fd_log_debug("A new session has been created\n");
		} else {
			fd_log_debug("A session with same id already existed\n");
		}
		#endif /* 0 */
		
		return s;
	}
	~session() {
		struct session * s = self;
		int ret = fd_sess_reclaim(&s);
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
		}
		return;
	}
	char * getsid() {
		int ret;
		char * sid = NULL;
		ret = fd_sess_getsid( $self, &sid);
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
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
			DI_ERROR(ret, NULL, NULL);
		}
	}
	void dump() {
		fd_sess_dump(0, $self);
	}
	void store(struct session_handler * handler, PyObject * state) {
		int ret;
		void * store = state;
		Py_XINCREF(state);
		ret = fd_sess_state_store_internal(handler, $self, (void *) &store);
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
		}
	}
	PyObject *  retrieve(struct session_handler * handler) {
		int ret;
		PyObject * state = NULL;
		ret = fd_sess_state_retrieve_internal(handler, $self, (void *) &state);
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
			return NULL;
		}
		if (state == NULL) {
			Py_INCREF(Py_None);
			return Py_None;
		}
		return state;
	}
}	

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
	struct fd_list * extract_li(int score = 0) {
		struct fd_list * li = NULL;
		fd_rtd_candidate_extract($self, &li, score);
		return li;
	}
	PyObject * extract(int score = 0) {
		struct fd_list * list = NULL, *li;
		PyObject * rl;
		fd_rtd_candidate_extract($self, &list, score);
		rl = PyList_New(0);
		SWIG_PYTHON_THREAD_BEGIN_BLOCK;
		for (li = list->next; li != list; li = li->next) {
			PyList_Append(rl, SWIG_NewPointerObj((void *)li, SWIGTYPE_p_rtd_candidate, 0 ));
		}
		Py_XINCREF(rl);
		SWIG_PYTHON_THREAD_END_BLOCK;
		
		return rl;
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



/****** MESSAGES *********/

struct msg {
};

%extend msg {
	msg() {
		DI_ERROR(EINVAL, PyExc_SyntaxError, "A DICT_COMMAND object parameter (or None) is required.");
		return NULL;
	}
	msg(struct dict_object * model, int flags = MSGFL_ALLOC_ETEID) {
		struct msg * m = NULL;
		int ret = fd_msg_new( model, flags, &m);
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
		}
		return m;
	}
	~msg() {
		int ret = fd_msg_free($self);
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
		}
	}
	%newobject create_answer;
	struct msg * create_answer(struct dictionary * dict = NULL, int flags = 0) {
		/* if dict is not provided, attempt to get it from the request model */
		struct dictionary * d = dict;
		struct msg * m = $self;
		int ret;
		if (!d) {
			struct dict_object * mo = NULL;
			ret = fd_msg_model($self, &mo);
			if (ret != 0) {
				DI_ERROR(ret, NULL, "Cannot guess the dictionary to use, please provide it as parameter.");
				return NULL;
			}
			ret = fd_dict_getdict ( mo, &d );
			if (ret != 0) {
				DI_ERROR(ret, NULL, "Cannot guess the dictionary to use, please provide it as parameter.");
				return NULL;
			}
		}
		ret = fd_msg_new_answer_from_req(d, &m, flags);
		if (ret != 0) {
			DI_ERROR(ret, NULL, "Cannot guess the dictionary to use, please provide it as parameter.");
			return NULL;
		}
		return m;
	}
}

struct avp {
};

%extend avp {
	avp() {
		DI_ERROR(EINVAL, PyExc_SyntaxError, "A DICT_AVP object parameter (or None) is required.");
		return NULL;
	}
	avp(struct dict_object * model, int flags = 0) {
		struct avp * a = NULL;
		int ret = fd_msg_avp_new( model, flags, &a);
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
		}
		return a;
	}
	~avp() {
		int ret = fd_msg_free($self);
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
		}
	}
}
	



%cstring_output_allocate_size(char ** swig_buffer, size_t * swig_len, free(*$1))
%inline %{
int fd_msg_bufferize_py ( struct msg * msg, char ** swig_buffer, size_t * swig_len ) {
	return fd_msg_bufferize(msg, (void *)swig_buffer, swig_len);
}
%};

