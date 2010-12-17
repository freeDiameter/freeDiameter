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

%nodefaultctor session_handler;
%extend session_handler {
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
	void store(struct session_handler * handler, PyObject * DISOWN) {
		int ret;
		void * store = DISOWN;
		Py_XINCREF(DISOWN);
		ret = fd_sess_state_store_internal(handler, $self, (void *) &store);
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
		}
	}
	%newobject retrieve;
	PyObject * retrieve(struct session_handler * handler) {
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

