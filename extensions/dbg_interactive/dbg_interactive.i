/* This interface file is processed by SWIG to create a python wrapper interface to freeDiameter framework. */
%module fDpy
%nothreadallow;
%begin %{
#define PY_SSIZE_T_CLEAN
/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@freediameter.net>							 *
*													 *
* Copyright (c) 2013, WIDE Project and NICT								 *
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
#undef HAVE_CLOCK_GETTIME
#include <freeDiameter/extension.h>
%}


/* Include standard types & functions used in freeDiameter headers */
%include <stdint.i>
//%include <cdata.i>
%include <cstring.i>
%include <typemaps.i>


/* Inline functions seems to give problems to SWIG -- just remove the inline definition */
%define __inline__ 
%enddef


/* Make some global-variables read-only (mainly to avoid warnings) */
%immutable fd_g_config;
%immutable peer_state_str;

#pragma SWIG nowarn=451

/*****************
 *  Exceptions  *
*****************/
%{
struct wrapper_error {
	int errnum;
	PyObject *exception;
	const char *text;
};
static pthread_key_t wrapper_error_key;
static pthread_once_t wrapper_error_once = PTHREAD_ONCE_INIT;
static int wrapper_key_error;
static void wrapper_error_init(void) {
	wrapper_key_error = pthread_key_create(&wrapper_error_key, NULL);
}
#define DI_ERROR(code, pycode, str) {	\
	struct wrapper_error *error = pthread_getspecific(wrapper_error_key); \
	fd_log_debug("[dbg_interactive] ERROR: %s: %s", __PRETTY_FUNCTION__, str ? str : strerror(code)); \
	error->errnum = code;		\
	error->exception = pycode;	\
	error->text = str;	\
}

#define DI_ERROR_MALLOC	\
	 DI_ERROR(ENOMEM, PyExc_MemoryError, NULL)

%}

%exception {
	struct wrapper_error error = { 0, NULL, NULL };
	struct wrapper_error *previous;
	int key_error = pthread_once(&wrapper_error_once, wrapper_error_init);
	if (key_error || wrapper_key_error) {
		PyErr_SetString(PyExc_RuntimeError, "Cannot initialize wrapper error state");
		SWIG_fail;
	}
	previous = pthread_getspecific(wrapper_error_key);
	if (pthread_setspecific(wrapper_error_key, &error)) {
		PyErr_SetString(PyExc_RuntimeError, "Cannot set wrapper error state");
		SWIG_fail;
	}
	$action
	if (pthread_setspecific(wrapper_error_key, previous)) {
		PyErr_SetString(PyExc_RuntimeError, "Cannot restore wrapper error state");
		SWIG_fail;
	}
	if (error.errnum) {
		const char * str = error.text ? error.text : strerror(error.errnum);
		PyObject * exc = error.exception;
		if (!exc) {
			switch (error.errnum) {
				case ENOMEM: exc = PyExc_MemoryError; break;
				case EINVAL: exc = PyExc_ValueError; break;
				default: exc = PyExc_RuntimeError;
			}
		}
		PyErr_SetString(exc, str);
		SWIG_fail;
	}
}


/***********************************
 Some types & typemaps for usability 
 ***********************************/

%typemap(in) (char *STRING, size_t LENGTH) (Py_ssize_t length, const char *buffer) {
	if (PyBytes_Check($input)) {
		char *bytes;
		if (PyBytes_AsStringAndSize($input, &bytes, &length) < 0)
			SWIG_fail;
		buffer = bytes;
	} else if (PyUnicode_Check($input)) {
		buffer = PyUnicode_AsUTF8AndSize($input, &length);
		if (!buffer)
			SWIG_fail;
	} else {
		PyErr_SetString(PyExc_TypeError, "Expected bytes or str");
		SWIG_fail;
	}
	$1 = ($1_ltype)buffer;
	$2 = (size_t)length;
}
%typemap(typecheck, precedence=SWIG_TYPECHECK_STRING) (char *STRING, size_t LENGTH) {
	$1 = PyBytes_Check($input) || PyUnicode_Check($input);
}
%typemap(freearg) (char *STRING, size_t LENGTH) "";
%apply (char *STRING, size_t LENGTH) { ( char * string, size_t len ) };
%apply (char *STRING, size_t LENGTH) { ( uint8_t * string, size_t len ) };

/* Generic typemap for functions that create something */
%typemap(in, numinputs=0,noblock=1) SWIGTYPE ** OUTPUT (void *temp = NULL) {
	$1 = (void *)&temp;
}
%typemap(argout,noblock=1) SWIGTYPE ** OUTPUT {
	%append_output(SWIG_NewPointerObj(*$1, $*1_descriptor, 0));
}

/* Case of the fd_*_dump functions */
%typemap(in,noblock=1,numinputs=0) (char ** buf, size_t *len, size_t *offset) ($*1_ltype temp = NULL, $*2_ltype tempn = 0) {
	$1 = &temp; $2 = &tempn; $3 = NULL;
}
%typemap(freearg,match="in") (char ** buf, size_t *len, size_t *offset) "";
%typemap(argout,noblock=1,fragment="SWIG_FromCharPtr")(char ** buf, size_t *len, size_t *offset) { 
  if (*$1) {
    %append_output(SWIG_FromCharPtr(*$1));
    free(*$1);					  	     
  }					  	     
}							     

/* Typemap to return a boolean value as output parameter */
%typemap(in, numinputs=0,noblock=1) int * BOOL_OUT (int temp) {
	$1 = &temp;
}
%typemap(argout,noblock=1) int * BOOL_OUT {
	PyObject * r;
	if (*$1)
		r = Py_True;
	else
		r = Py_False;
	Py_XINCREF(r);
	%append_output(r);
}

/* To allow passing callback functions defined in python */
%typemap(in) PyObject *PyCb {
	if (!$input || ($input == Py_None)) {
		$1 = NULL;
	} else {
		if (!PyCallable_Check($input)) {
			PyErr_SetString(PyExc_TypeError, "Need a callable object!");
			SWIG_fail;
		}
		$1 = $input;
	}
}

%{
/* Forward declaration for the peers module */
static void fd_add_cb(struct peer_info *peer, void *data);

/* This one gives problems when included from the header file */
void fd_log_va( int, const char *, va_list);
void fd_log_deprecated( int level, const char *format, ... ) MARK_DEPRECATED
{ 
	va_list ap;
	va_start(ap, format);
	fd_log_va(level, format, ap);
	va_end(ap);
}

%}

/* Overwrite declaration to apply typemaps */
int fd_sess_fromsid ( uint8_t * string, size_t len, struct session ** OUTPUT, int * BOOL_OUT);



/*********************************************************
 Now, create wrappers for (almost) all objects from fD API 
 *********************************************************/
%include "freeDiameter/freeDiameter-host.h"
%include "freeDiameter/libfdproto.h"
%include "freeDiameter/libfdcore.h"

/* Most of the functions from the API are not directly usable "as is".
See the specific following files and the dbg_interactive.py.sample file
for more usable python-style versions.
*/

%include "lists.i"
%include "dictionary.i"
%include "sessions.i"
%include "routing.i"
%include "messages.i"
%include "dispatch.i"
%include "queues.i"

%include "peers.i"
%include "events.i"
%include "endpoints.i"
%include "hooks.i"

%include "posix.i"
