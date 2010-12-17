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

/****** PEERS *********/

%{
static void fd_add_cb(struct peer_info *peer, void *data) {
	/* Callback called when the peer connection completes (or fails) */
	PyObject *PyPeer, *PyFunc;
	PyObject *result = NULL;
	
	if (!data) {
		TRACE_DEBUG(INFO, "Internal error: missing callback\n");
		return;
	}
	PyFunc = data;
	
	SWIG_PYTHON_THREAD_BEGIN_BLOCK;
	
	/* Convert the argument */
	PyPeer  = SWIG_NewPointerObj((void *)peer,     SWIGTYPE_p_peer_info,     0 );
	
	/* Call the function */
	result = PyEval_CallFunction(PyFunc, "(O)", PyPeer);
	
	Py_XDECREF(result);
	Py_XDECREF(PyFunc);
	
	SWIG_PYTHON_THREAD_END_BLOCK;
	return;
}
%}

%extend peer_info {
	/* Wrapper around fd_peer_add to allow calling the python callback */
	void add(PyObject * PyCb=NULL) {
		int ret;
		
		if (PyCb) {
			Py_XINCREF(PyCb);
			ret = fd_peer_add ( $self, "dbg_interactive", fd_add_cb, PyCb );
		} else {
			ret = fd_peer_add ( $self, "dbg_interactive", NULL, NULL );
		}
		if (ret != 0) {
			DI_ERROR(ret, NULL, NULL);
		}
	}

	/* Add an endpoint */
	void add_endpoint(char * endpoint) {
		fd_log_debug("What is the best way in python to pass an endpoint? (ip + port)");
	}

}
