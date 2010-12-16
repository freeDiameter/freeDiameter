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

%nodefaultctor dict_object;
struct dict_object {
};

%extend dict_object {
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
