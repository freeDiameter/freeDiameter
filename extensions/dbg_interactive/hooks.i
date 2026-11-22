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

/* Do not include this directly, use dbg_interactive.i instead */

/****** HOOKS *********/


%{
struct di_hook_hdl {
	struct fd_hook_hdl *hdl;
	PyObject *callback;
};

%}

struct fd_hook_data_hdl {
};

%nodefaultctor fd_hook_data_hdl;
%extend fd_hook_data_hdl {
	fd_hook_data_hdl() {
		DI_ERROR(ENOTSUP, PyExc_NotImplementedError, "Python per-message hook data cannot be safely unregistered");
		return NULL;
	}
}


/* Now the hook itself */
%{
static void call_the_python_hook_callback(enum fd_hook_type type, struct msg * msg, struct peer_hdr * peer, void * other, struct fd_hook_permsgdata *pmd, void * regdata) {
	PyObject *cb, *result = NULL;
	PyObject *PyType, *PyMsg, *PyPeer, *PyOther;
	
	if (!regdata) {
		LOG_E("Internal error: missing the callback!");
		return;
	}
	cb = ((struct di_hook_hdl *)regdata)->callback;

	SWIG_PYTHON_THREAD_BEGIN_BLOCK;
	/* Convert the arguments */
	PyType = PyLong_FromLong(type);
	PyMsg  = SWIG_NewPointerObj((void *) msg,     SWIGTYPE_p_msg,     0 );
	PyPeer = SWIG_NewPointerObj((void *) peer,    SWIGTYPE_p_peer_hdr,     0 );
	if (other == NULL) {
		PyOther=Py_None;
		Py_INCREF(Py_None);
	} else {
		switch (type) {
		case HOOK_DATA_RECEIVED:
			PyOther= SWIG_NewPointerObj( other, SWIGTYPE_p_fd_cnx_rcvdata, 0 );
			break;
			
		case HOOK_MESSAGE_RECEIVED:
		case HOOK_MESSAGE_ROUTING_ERROR:
		case HOOK_MESSAGE_DROPPED:
		case HOOK_PEER_CONNECT_FAILED:
			PyOther= SWIG_NewPointerObj( other, SWIGTYPE_p_char, 0 );
			break;
			
		case HOOK_MESSAGE_PARSING_ERROR:
			if (msg) {
				PyOther= SWIG_NewPointerObj( other, SWIGTYPE_p_char, 0 );
			} else {
				PyOther= SWIG_NewPointerObj( other, SWIGTYPE_p_fd_cnx_rcvdata, 0 );
			}
			break;
		default:
			LOG_E("Internal error: got a value of *other");
			PyOther = Py_None;
			Py_INCREF(PyOther);
		}
		
	}
	
	/* Call the function */
	if (PyType && PyMsg && PyPeer && PyOther)
		result = PyObject_CallFunction(cb, "(OOOOO)", PyType, PyMsg, PyPeer, PyOther, Py_None);
	if (!result) {
		PyErr_WriteUnraisable(cb);
		LOG_E("[dbg_interactive] Python hook callback failed");
	}
	Py_XDECREF(result);
	Py_XDECREF(PyType);
	Py_XDECREF(PyMsg);
	Py_XDECREF(PyPeer);
	Py_XDECREF(PyOther);
	SWIG_PYTHON_THREAD_END_BLOCK;
}
%}



%rename(fd_hook_hdl) di_hook_hdl;
struct di_hook_hdl {
};

%nodefaultctor di_hook_hdl;
%extend di_hook_hdl {
	di_hook_hdl(uint32_t type_mask, PyObject * PyCb, struct fd_hook_data_hdl *datahdl = NULL) {
		struct di_hook_hdl *hdl;
		int ret;
		if (!PyCb) {
			DI_ERROR(EINVAL, NULL, "The callback must be provided");
			return NULL;
		}
		if (datahdl) {
			DI_ERROR(ENOTSUP, PyExc_NotImplementedError, "Python per-message hook data cannot be safely unregistered");
			return NULL;
		}
		hdl = calloc(1, sizeof(*hdl));
		if (!hdl) {
			DI_ERROR_MALLOC;
			return NULL;
		}
		Py_INCREF(PyCb);
		hdl->callback = PyCb;
		ret = fd_hook_register(type_mask, call_the_python_hook_callback, hdl, NULL, &hdl->hdl);
		if (ret != 0) {
			Py_DECREF(PyCb);
			free(hdl);
			DI_ERROR(ret, NULL, NULL);
			return NULL;
		}
		return hdl;
	}
	~di_hook_hdl() {
		int ret;
		Py_BEGIN_ALLOW_THREADS
		ret = fd_hook_unregister(self->hdl);
		Py_END_ALLOW_THREADS
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
			return;
		}
		Py_DECREF(self->callback);
		free(self);
	}
}
