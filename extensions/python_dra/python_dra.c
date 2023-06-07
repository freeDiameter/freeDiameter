#include <freeDiameter/extension.h>
#include <python3.10/Python.h>
#include <stdlib.h>
#include <signal.h>
#include "python_dra_config.h"
#include <stdbool.h>


#define MODULE_NAME "python_dra"


/* Do not directly interact with the fields in this struct */
typedef struct
{
    bool initialised;

    char directoryPath[MAX_DIRECTORY_PATH];
    char moduleName[MAX_MODULE_NAME];
    char functionName[MAX_FUNCTION_NAME];
} PythonDraState;


/* Static objects */
static struct fd_rt_out_hdl *python_dra_requests_handle = NULL;
static struct fd_rt_fwd_hdl *python_dra_answers_handle = NULL;
static pthread_rwlock_t python_dra_lock;
static PythonDraState pythonDraState = {0};


/* Static function prototypes */
static PyObject* getPeerSupportedApplicationsPyList(const char *diamid);
static PyObject* getPeersPyDict(struct fd_list * candidates);
static int pythonDraInitialise(
    PythonDraState *const state,
    char const *const directoryPath,
    char const *const moduleName,
    char const *const functionName);
static int pythonDraFinalise(PythonDraState *const state);
static int pythonDraSetContext(
    PythonDraState *const state,
    char const *const directoryPath,
    char const *const moduleName,
    char const *const functionName);
static int python_dra_entry(char *conffile);
static int updateAvpValue(PyObject* py_avp_value, struct avp *avp);
static int updateAvps(PyObject* py_update_avps_list, struct avp *first_avp);
static int addAvpsToMsgOrAvp(PyObject* py_add_avps_list, msg_or_avp *msg_or_avp);
static int removeAvpsFromMsg(PyObject* py_add_avps_list, struct avp *first_avp);
static struct rtd_candidate * getCandidate(struct fd_list * candidates, const char* diamid);
static int updatePriority(PyObject* py_update_peer_priority_dict, struct fd_list * candidates);
static void pythonDraUpdate(PyObject* py_result_dict, struct msg *msg, struct fd_list * candidates);
static int python_dra(void * cbdata, struct msg ** msg, struct fd_list * candidates);
static int python_dra_answers(void * cbdata, struct msg ** msg);
static PyObject* msgToPyDict(struct msg *msg);
static PyObject* avpChainToPyList(struct avp *first_avp);
static PyObject* avpToPyDict(struct avp *avp);
static PyObject* getPyAvpValue(struct avp *avp, enum dict_avp_basetype avp_type, union avp_value * avp_value);
static PyObject* msgHeaderToPyDict(struct msg *msg);
static PyObject* pythonDraTransformValue(PyObject* py_msg_dict, PyObject* py_peers_dict);
static PyObject* pythonDraGetFunction(void);
static PyObject *pythonDraGetModule(void);
static struct avp *getNextAVP(struct avp *avp);
static struct avp *getFirstAVP(msg_or_avp *msg);
static struct avp_hdr *getAvpHeader(struct avp *avp);
static struct dict_object *getAVP_Model(struct avp *avp);
static bool getAvpDictData(struct dict_object *model, struct dict_avp_data* avpDictData);
static struct msg_hdr* getMsgHeader(struct msg *msg);
static struct avp* findAvp(struct avp *first_avp, uint32_t avp_code, uint32_t avp_vendor);
static struct avp* newAvpInstance(uint32_t avp_code, uint32_t avp_vendor);
static bool setAvpValue(union avp_value* value, struct avp* avp);
static bool removeAvp(struct avp* avp);
static bool addAvp(struct avp* avp, msg_or_avp* msg_or_avp);
static bool applyDictToMsgOrAvp(msg_or_avp* msg_or_avp);
static enum dict_avp_basetype getAvpBasetype(struct avp* avp);
static bool isAvp(msg_or_avp* msg_or_avp);


/* Define the entry point function */
EXTENSION_ENTRY("python_dra", python_dra_entry);


/* Cleanup the callbacks */
void fd_ext_fini(void)
{
	TRACE_ENTRY();
	
	/* Unregister the cb */
	fd_rt_out_unregister(python_dra_requests_handle, NULL);
	fd_rt_fwd_unregister(python_dra_answers_handle, NULL);
	
	/* Destroy the data */
	/* Disabled due to issues with GIL,
	 * lets rely on the OS can clean up */
	// pythonDraFinalise(&pythonDraState);

	pthread_rwlock_destroy(&python_dra_lock);

	/* Done */
	return ;
}

/* Only used during development */
void debug_print(struct avp *first_avp, int depth) {
	if (NULL == first_avp) {
		fd_log_notice("first avp was null\n");
		return;
	}

	for (struct avp *avp = first_avp; NULL != avp; avp = getNextAVP(avp))
	{
		struct avp_hdr *AVP_Header = getAvpHeader(avp);

		if (NULL == AVP_Header) {
			fd_log_notice("Error: AVP has no header\n");
			continue;
		}

		for (int i = 0; i < depth; i++) {
			fd_log_notice("\t");
		}

		fd_log_notice("[debug_print] Code:%d, Vendor:%d\n", AVP_Header->avp_code, AVP_Header->avp_vendor);
		struct dict_avp_data avp_dict_data = {0};
		struct dict_object *model = getAVP_Model(avp);
		if (getAvpDictData(model, &avp_dict_data)) {
			if (AVP_TYPE_GROUPED == avp_dict_data.avp_basetype) {
				debug_print(getFirstAVP(avp), depth + 1);
			}
		}
	}
}

static PyObject* getPeerSupportedApplicationsPyList(const char *diamid) {
	struct fd_list * li_outer = NULL;
	struct fd_list * li_inner = NULL;
	PyObject* py_supported_applications_list = PyList_New(0);

	if (pthread_rwlock_rdlock(&fd_g_peers_rw) != 0)
	{
		fd_log_notice("%s: write-lock failed, aborting", MODULE_NAME);
		return NULL;
	}

	/* For each peer */
	for (li_outer = fd_g_peers.next; li_outer != &fd_g_peers; li_outer = li_outer->next) {
		struct peer_hdr* p = (struct peer_hdr*)li_outer->o;

		if (!strcmp(diamid, p->info.pi_diamid)) {
			/* For each supported app in peer */
			for (li_inner = &p->info.runtime.pir_apps; li_inner->next != &p->info.runtime.pir_apps; li_inner = li_inner->next) {
				struct fd_app* peer_app_item = (struct fd_app*)(li_inner->next);
				PyObject* py_app_id = PyLong_FromLong(peer_app_item->appid);

				PyList_Append(py_supported_applications_list, py_app_id);
				Py_DECREF(py_app_id);
			}
			break;
		}
	}

	if (pthread_rwlock_unlock(&fd_g_peers_rw) != 0)
	{
		fd_log_notice("%s: write-lock failed, aborting", MODULE_NAME);
		return NULL;
	}

	return py_supported_applications_list;
}

/* Caller responsible for calling Py_XDECREF() on result if not NULL */
static PyObject* getPeersPyDict(struct fd_list * candidates) {
	struct fd_list * li;
	
	if (NULL == candidates) {
		return NULL;
	}
	
	PyObject* py_peers_dict_parent = PyDict_New();
	for (li = candidates->next; li != candidates; li = li->next) {
		struct rtd_candidate * cand = (struct rtd_candidate *)li;

		PyObject* py_peers_dict_child = PyDict_New();
		PyObject* py_realm = PyUnicode_FromString(cand->realm);
		PyObject* py_supported_applications_list = getPeerSupportedApplicationsPyList(cand->diamid);
		PyObject* py_priority = PyLong_FromLong(cand->score);

		if ((NULL != py_peers_dict_child)            &&
			(NULL != py_realm)                       &&
			(NULL != py_supported_applications_list) &&
			(NULL != py_priority)) {
			PyDict_SetItemString(py_peers_dict_child, "realm", py_realm);
			PyDict_SetItemString(py_peers_dict_child, "supported_applications", py_supported_applications_list);
			PyDict_SetItemString(py_peers_dict_child, "priority", py_priority);
			PyDict_SetItemString(py_peers_dict_parent, cand->diamid, py_peers_dict_child);
		} else {
			fd_log_error("getPeersPyDict: Failed to create peers PyObject\n");
		}
		
		Py_XDECREF(py_peers_dict_child);
		Py_XDECREF(py_realm);
		Py_XDECREF(py_supported_applications_list);
		Py_XDECREF(py_priority);
	}

	return py_peers_dict_parent;
}

static int pythonDraInitialise(
    PythonDraState *const state,
    char const *const directoryPath,
    char const *const moduleName,
    char const *const functionName)
{
    int isFailed = 0;

    if (NULL != state)
    {
        if (0 == pythonDraSetContext(state, directoryPath, moduleName, functionName))
        {
            setenv("PYTHONPATH", state->directoryPath, 1);
            Py_Initialize();
            state->initialised = true;

            isFailed = 0;
        }
    }

    return isFailed;
}

static int pythonDraFinalise(PythonDraState *const state) {
    int isFailed = 0;
    if (NULL != state)
    {
        if (0 != Py_FinalizeEx())
        {
            fd_log_error("pythonDraFinalise: Failed to finalise cpython");
        }
        else
        {
            state->initialised = false;
            isFailed = 0;
        }
    }
    return isFailed;
}

static int pythonDraSetContext(
    PythonDraState *const state,
    char const *const directoryPath,
    char const *const moduleName,
    char const *const functionName)
{
    int isFailed = 1;

    if ((NULL != directoryPath) &&
        (NULL != moduleName) &&
        (NULL != functionName))
    {
        strncpy(state->directoryPath, directoryPath, MAX_DIRECTORY_PATH);
        strncpy(state->moduleName, moduleName, MAX_MODULE_NAME);
        strncpy(state->functionName, functionName, MAX_FUNCTION_NAME);

        isFailed = 0;
    }

    return isFailed;
}

/* Entry point called when loading the module */
static int python_dra_entry(char *conffile) {
	TRACE_ENTRY("%p", conffile);
	int ret = 0;
	char directoryPath[MAX_DIRECTORY_PATH] = "";
	char moduleName[MAX_MODULE_NAME] = "";
	char functionName[MAX_FUNCTION_NAME] = "";

	pthread_rwlock_init(&python_dra_lock, NULL);
	if (pthread_rwlock_wrlock(&python_dra_lock) != 0)
	{
		fd_log_notice("%s: write-lock failed, aborting", MODULE_NAME);
		return EDEADLK;
	}

	if (0 != parseConfig(conffile, directoryPath, moduleName, functionName))
	{
		fd_log_notice("%s: Encountered errors when parsing config file", MODULE_NAME);
		fd_log_error(
			"Config file should be 3 lines long and look like the following:\n"
			"DirectoryPath = \".\"        # Look in current dir\n"
			"ModuleName = \"script\"      # Note no .py extension\n"
			"FunctionName = \"transform\" # Python function to call");

		return EINVAL;
	}

	pythonDraInitialise(&pythonDraState, directoryPath, moduleName, functionName);

	if (pthread_rwlock_unlock(&python_dra_lock) != 0)
	{
		fd_log_notice("%s: write-unlock failed, aborting", MODULE_NAME);
		return EDEADLK;
	}

	if (0 != (ret = fd_rt_fwd_register(python_dra_answers, NULL, RT_FWD_ANS, &python_dra_answers_handle)))
	{
		fd_log_error("Cannot register python_dra_answers callback handler");
		return ret;
	}

	if (0 != (ret = fd_rt_out_register(python_dra, NULL, 0, &python_dra_requests_handle)))
	{
		fd_log_error("Cannot register python_dra_requests callback handler");
		return ret;
	}

	fd_log_notice("Extension 'PythonDRA' initialized");
	return 0;
}

static int updateAvpValue(PyObject* py_avp_value, struct avp *avp) {
	int errors = 0;
	struct dict_avp_data avp_dict_data = {0};
	union avp_value avp_value = {0};

	if ((NULL == py_avp_value) ||
	    (NULL == avp)) {
		fd_log_error("updateAvpValue: Invaid parameter");
		return 1;
	}

	struct dict_object *model = getAVP_Model(avp);

	if (!getAvpDictData(model, &avp_dict_data)) {
		fd_log_error("Error: Trying to python tranform unknown AVP (not in dictionary)");
		return 1;
	}

	switch (avp_dict_data.avp_basetype) {
		case AVP_TYPE_GROUPED: 
		{
			/* Caller should be handling grouped */
			fd_log_error("updateAvpValue: Error: Grouped AVPs should not have non-AVP values '%s'", avp_dict_data.avp_name);
			errors += 1;
			break;
		}
		case AVP_TYPE_OCTETSTRING:
		{
			if (PyUnicode_Check(py_avp_value)) {
				const char* data = PyUnicode_AsUTF8(py_avp_value);

				if (NULL != data) {
					avp_value.os.data = (uint8_t*)data;
					avp_value.os.len = strlen(data);
				}
			} else {
				fd_log_error("updateAvpValue: error expected sting value for AVP '%s'", avp_dict_data.avp_name);
				errors += 1;
			}
			break;
		}
		case AVP_TYPE_INTEGER32: 
		{
			if (PyLong_Check(py_avp_value)) {
				avp_value.i32 = PyLong_AsLong(py_avp_value);
			} else {
				fd_log_error("updateAvpValue: error expected integer value for AVP '%s'", avp_dict_data.avp_name);
				errors += 1;
			}
			break;
		}
		case AVP_TYPE_INTEGER64: 
		{
			if (PyLong_Check(py_avp_value)) {
				avp_value.i64 = PyLong_AsLongLong(py_avp_value);
			} else {
				fd_log_error("updateAvpValue: error expected integer value for AVP '%s'", avp_dict_data.avp_name);
				errors += 1;
			}
			break;
		}
		case AVP_TYPE_UNSIGNED32: 
		{
			if (PyLong_Check(py_avp_value)) {
				avp_value.u32 = PyLong_AsUnsignedLong(py_avp_value);
			} else {
				fd_log_error("updateAvpValue: error expected integer value for AVP '%s'", avp_dict_data.avp_name);
				errors += 1;
			}
			break;
		}
		case AVP_TYPE_UNSIGNED64: 
		{
			if (PyLong_Check(py_avp_value)) {
				avp_value.u64 = PyLong_AsUnsignedLongLong(py_avp_value);
			} else {
				fd_log_error("updateAvpValue: error expected integer value for AVP '%s'", avp_dict_data.avp_name);
				errors += 1;
			}
			break;
		}
		case AVP_TYPE_FLOAT32: 
		{
			if (PyFloat_Check(py_avp_value)) {
				avp_value.f32 = (float)PyFloat_AsDouble(py_avp_value);
			} else {
				fd_log_error("updateAvpValue: error expected floating point value for AVP '%s'", avp_dict_data.avp_name);
				errors += 1;
			}
			break;
		}
		case AVP_TYPE_FLOAT64: 
		{
			if (PyFloat_Check(py_avp_value)) {
				avp_value.f64 = PyFloat_AsDouble(py_avp_value);
			} else {
				fd_log_error("updateAvpValue: error expected floating point value for AVP '%s'", avp_dict_data.avp_name);
				errors += 1;
			}
			break;
		}
		default:
		{
			fd_log_error("updateAvpValue: Unknown AVP value for '%s', this should never be hit", avp_dict_data.avp_name);
			errors += 1;
			break;
		}
	}

	if (!setAvpValue(&avp_value, avp)) {
		fd_log_error("updateAvpValue: Failed to set AVP value");
		errors += 1;
	}

	return errors;
}

static int updateAvps(PyObject* py_update_avps_list, struct avp *first_avp) {
	int errors = 0;
	struct avp *current_avp = NULL;

	if ((NULL == py_update_avps_list) || 
	    (NULL == first_avp)) {
		fd_log_error("updateAvps: Invaid parameter");
		return 1;
	}
	current_avp = first_avp;

	Py_ssize_t size = PyList_Size(py_update_avps_list);
	for (Py_ssize_t i = 0; i < size; i++) {
		/* PyDict_GetItemString and PyList_GetItem return borrowed
		 * references so we don't need to call Py_DECREF */
		PyObject* py_avp_dict = PyList_GetItem(py_update_avps_list, i);
    	PyObject* py_avp_code = PyDict_GetItemString(py_avp_dict, "code");
    	PyObject* py_avp_vendor = PyDict_GetItemString(py_avp_dict, "vendor");
    	PyObject* py_avp_value = PyDict_GetItemString(py_avp_dict, "value");

		if ((NULL == py_avp_dict)   ||  
			(NULL == py_avp_code)   ||  
			(NULL == py_avp_vendor) ||    
			(NULL == py_avp_value)) {
			/* Note this will also refer to a child index. 
			 * E.g. If parent index is 2 and were at the 
			 * child index of 1, then 1 will be printed. */
			fd_log_error("updateAvps: Failed to get python AVP item from python dict at index %i", i);
			errors += 1;
            continue;
		}

		uint32_t avp_code = PyLong_AsUnsignedLong(py_avp_code);
		uint32_t avp_vendor = PyLong_AsUnsignedLong(py_avp_vendor);
		current_avp = findAvp(first_avp, avp_code, avp_vendor);

		/* If this is a dict then we are changing a child */
		if (PyList_Check(py_avp_value)) {
			errors += updateAvps(py_avp_value, getFirstAVP(current_avp));
		} else {
			errors += updateAvpValue(py_avp_value, current_avp);
		}
	}

	return errors;
}

static int addAvpsToMsgOrAvp(PyObject* py_add_avps_list, msg_or_avp *msg_or_avp) {
	int errors = 0;
	Py_ssize_t size = 0; 
	struct avp *first_avp = NULL;
	
	if ((NULL == py_add_avps_list) ||
	    (NULL == msg_or_avp)) {
		fd_log_error("addAvpsToMsgOrAvp: Invaid parameter");
		return 1;
	}
	first_avp = getFirstAVP(msg_or_avp);
	size = PyList_Size(py_add_avps_list);

	/* Loop through the python dict */
	for (Py_ssize_t i = 0; i < size; i++) {
		/* PyDict_GetItemString and PyList_GetItem return borrowed
		 * references so we don't need to call Py_DECREF */
		PyObject* py_avp_dict = PyList_GetItem(py_add_avps_list, i);
    	PyObject* py_avp_code = PyDict_GetItemString(py_avp_dict, "code");
    	PyObject* py_avp_vendor = PyDict_GetItemString(py_avp_dict, "vendor");
    	PyObject* py_avp_value = PyDict_GetItemString(py_avp_dict, "value");

		if ((NULL == py_avp_dict)   ||
		    (NULL == py_avp_code)   ||
		    (NULL == py_avp_vendor) ||
		    (NULL == py_avp_value)) {
			/* Note this will also refer to a child index. 
			 * E.g. If parent index is 2 and were at the 
			 * child index of 1, then 1 will be printed. */
			fd_log_error("addAvpsToMsgOrAvp: Failed to get python AVP item from python dict at index %i", i);
			errors += 1;
            continue;
		}

		uint32_t avp_code = PyLong_AsUnsignedLong(py_avp_code);
		uint32_t avp_vendor = PyLong_AsUnsignedLong(py_avp_vendor);
        struct avp* existing_avp = findAvp(first_avp, avp_code, avp_vendor);
        struct avp* current_avp = NULL;
		
		/* If AVP doesnt exist, make a new one */
		if (NULL == existing_avp) {
			current_avp = newAvpInstance(avp_code, avp_vendor);
		} else {
			current_avp = existing_avp;
		}

		/* If value is a list then we're adding children,
		 * otherwise set the AVP value */
		if (PyList_Check(py_avp_value)) {
			errors += addAvpsToMsgOrAvp(py_avp_value, current_avp);
		} else {
			errors += updateAvpValue(py_avp_value, current_avp);
		}

		/* Finally, if we created a new AVP then we need to add it
		 * to the parent msg or avp */
		if (NULL == existing_avp) {
			if (!addAvp(current_avp, msg_or_avp)) {
				fd_log_error("addAvpsToMsgOrAvp: Failed to add AVP to msg or AVP for [C:%i, V:%i]", avp_code, avp_vendor);
				errors += 1;
			}
		}
	}

    return errors;
}

static int removeAvpsFromMsg(PyObject* py_add_avps_list, struct avp *first_avp) {
    int errors = 0;
	Py_ssize_t size = PyList_Size(py_add_avps_list);

	for (Py_ssize_t i = 0; i < size; i++) {
		/* PyDict_GetItemString and PyList_GetItem return borrowed
		 * references so we don't need to call Py_DECREF */
		PyObject* py_avp_dict = PyList_GetItem(py_add_avps_list, i);
    	PyObject* py_avp_code = PyDict_GetItemString(py_avp_dict, "code");
    	PyObject* py_avp_vendor = PyDict_GetItemString(py_avp_dict, "vendor");
    	PyObject* py_avp_value = PyDict_GetItemString(py_avp_dict, "value");

		if ((NULL == py_avp_dict)   ||
		    (NULL == py_avp_code)   ||
		    (NULL == py_avp_vendor)) {
			/* Note this will also refer to a child index. 
			 * E.g. If parent index is 2 and were at the 
			 * child index of 1, then 1 will be printed. */
			fd_log_error("removeAvpsFromMsg: Failed to get python AVP item from python dict at index %i", i);
			errors += 1;
            continue;
		}

		uint32_t avp_code = PyLong_AsUnsignedLong(py_avp_code);
		uint32_t avp_vendor = PyLong_AsUnsignedLong(py_avp_vendor);
		struct avp * avp = findAvp(first_avp, avp_code, avp_vendor);

		/* If this is a list with children then we are removing a child */
		if ((NULL != py_avp_value)       &&
			(PyList_Check(py_avp_value)) && 
		    (0 < PyList_Size(py_avp_value))) {
			errors += removeAvpsFromMsg(py_avp_value, getFirstAVP(avp));
		} else {
			if (avp == first_avp) {
				/* If we're removing the head avp then we 
				 * need to have a reference to the new head */
				first_avp = getNextAVP(first_avp);
			}

			if (!removeAvp(avp)) {
				fd_log_error("removeAvpsFromMsg: Failed to remove AVP [Code: %i, Vendor: %i]", avp_code, avp_vendor);
			}
		}
	}

    return errors;
}

static struct rtd_candidate * getCandidate(struct fd_list * candidates, const char* diamid) {
	struct rtd_candidate *found = NULL;
	struct fd_list * li;

	if ((NULL == candidates) ||
	    (NULL == diamid)) {
		return NULL;
	}

	for (li = candidates->next; li != candidates; li = li->next) {
		struct rtd_candidate * cand = (struct rtd_candidate *)li;

		if (!strcmp(cand->diamid, diamid)) {
			found = cand;
			break;
		}
	}

	return found;
}

static int updatePriority(PyObject* py_update_peer_priority_dict, struct fd_list * candidates) {
	int errors = 0;
	Py_ssize_t pos = 0;
    PyObject* py_key = NULL;
	PyObject* py_value = NULL;

	if (NULL == candidates) {
		fd_log_error("updatePriority: Cannot update priority as no candidates exist");
		return 1;
	}

	/* PyDict_Next will set py_key and py_value with borrowed 
	 * values so we don't need to call Py_DECREF for them. */
    while (PyDict_Next(py_update_peer_priority_dict, &pos, &py_key, &py_value)) {
        PyObject* py_key_str = PyObject_Str(py_key);
		const char* cand_str = PyUnicode_AsUTF8(py_key_str);

		struct rtd_candidate* cand = getCandidate(candidates, cand_str);
        Py_XDECREF(py_key_str);
		
		if (NULL == cand) {
			fd_log_error("updatePriority: Failed to get candidate for '%s'", cand_str);
			errors += 1;
			continue;
		}

		if (!PyLong_Check(py_value)) {
			fd_log_error("updatePriority: Expected an integer '%s'", cand_str);
			errors += 1;
			continue;
		}

        long val = PyLong_AsLong(py_value);
		cand->score = (int)val;
		fd_log_notice("updatePriority: Priority for '%s' is now %i", cand_str, cand->score);
    }

	return errors;
}

static void pythonDraUpdate(PyObject* py_result_dict, struct msg *msg, struct fd_list * candidates) {
	int errors = 0;
	struct avp *first_avp = NULL;

	if ((NULL == py_result_dict) ||
	    (NULL == msg))
	{
		fd_log_error("pythonDraUpdate: Invaid parameter");
		return;
	}

	/* PyDict_GetItemString returns a borrowed references
	 * so we don't need to call Py_DECREF */
	PyObject* py_remove_avps_list = PyDict_GetItemString(py_result_dict, "remove_avps");
	PyObject* py_update_avps_list = PyDict_GetItemString(py_result_dict, "update_avps");
	PyObject* py_add_avps_list = PyDict_GetItemString(py_result_dict, "add_avps");
	PyObject* py_update_peer_priority_dict = PyDict_GetItemString(py_result_dict, "update_peer_priorities");

	if (py_remove_avps_list) {
		errors = 0;
 		first_avp = getFirstAVP(msg);
		errors += removeAvpsFromMsg(py_remove_avps_list, first_avp);
		Py_DECREF(py_remove_avps_list);
		if (0 < errors) {
			fd_log_error("pythonDraUpdate: Got %i errors when removing AVPs from msg", errors);
		}
	}

	if (py_update_avps_list) {
		errors = 0;
 		first_avp = getFirstAVP(msg);
		errors += updateAvps(py_update_avps_list, first_avp);
		Py_DECREF(py_update_avps_list);
		if (0 < errors) {
			fd_log_error("pythonDraUpdate: Got %i errors when updating AVPs from msg", errors);
		}
	}

	if (py_add_avps_list) {
		errors = 0;
		errors += addAvpsToMsgOrAvp(py_add_avps_list, msg);
		Py_DECREF(py_add_avps_list);
		if (0 < errors) {
			fd_log_error("pythonDraUpdate: Got %i errors when adding AVPs to msg", errors);
		}
	}

	if (py_update_peer_priority_dict) {
		errors = 0;
		errors += updatePriority(py_update_peer_priority_dict, candidates);
		Py_DECREF(py_update_peer_priority_dict);
		if (0 < errors) {
			fd_log_error("pythonDraUpdate: Got %i errors when updating peer priority", errors);
		}
	}

	Py_DECREF(py_result_dict);
}

static int python_dra(void * cbdata, struct msg ** msg, struct fd_list * candidates) {
	PyObject* py_result_dict = NULL;
	PyObject* py_msg_dict = NULL;
	PyObject* py_peers_dict = NULL;

	if (!pythonDraState.initialised) {
		fd_log_error("%s: python_dra called with uninitialised state, returning error", MODULE_NAME);
		return errno;
	}

	if (pthread_rwlock_wrlock(&python_dra_lock) != 0)
	{
		fd_log_error("%s: locking failed, aborting message", MODULE_NAME);
		return errno;
	}

	/* Convert from C to python */
	if (*msg) {
		py_msg_dict = msgToPyDict(*msg);
		py_peers_dict = getPeersPyDict(candidates);
	}

	/* Call python function */
	if (py_msg_dict) {
		py_result_dict = pythonDraTransformValue(py_msg_dict, py_peers_dict);
	}

	/* Convert from python to C and apply changes */
	if (py_result_dict) {
		pythonDraUpdate(py_result_dict, *msg, candidates);
	}

	Py_XDECREF(py_result_dict);
	Py_XDECREF(py_msg_dict);
	Py_XDECREF(py_peers_dict);

	if (pthread_rwlock_unlock(&python_dra_lock) != 0)
	{
		fd_log_error("%s: unlocking failed, returning error", MODULE_NAME);
		return errno;
	}

	return 0;
}

static int python_dra_answers(void * cbdata, struct msg ** msg) {
	return python_dra(cbdata, msg, NULL);
}

/* Caller responsible for calling Py_XDECREF() on result if not NULL */
static PyObject* msgToPyDict(struct msg *msg) {
	PyObject* py_msg_dict = msgHeaderToPyDict(msg);

	if (py_msg_dict) {
		struct avp *first_avp = getFirstAVP(msg);
		PyObject* py_avp_list = avpChainToPyList(first_avp);

		if (py_avp_list) {
			PyDict_SetItemString(py_msg_dict, "avp_list", py_avp_list);
			Py_DECREF(py_avp_list);
		} else {
			fd_log_error("msgToPyDict: Failed to parse C avp chain to pythonDra list");
		}
	}

	return py_msg_dict;
}

/* Caller responsible for calling Py_XDECREF() on result if not NULL */
static PyObject* avpChainToPyList(struct avp *first_avp) {
	PyObject* py_avp_list = PyList_New(0);

	for (struct avp *avp = first_avp; NULL != avp; avp = getNextAVP(avp))
	{
		PyObject* py_avp_dict = avpToPyDict(avp);

		if (py_avp_dict) {
			PyList_Append(py_avp_list, py_avp_dict);
			Py_DECREF(py_avp_dict);
		}
	}

	return py_avp_list;
}

/* Caller responsible for calling Py_XDECREF() on result if not NULL */
static PyObject* avpToPyDict(struct avp *avp) {
	struct dict_avp_data avp_dict_data = {0};
	struct avp_hdr *avp_header = NULL;
	struct dict_object *model = NULL;
	PyObject* py_avp_dict = NULL; 
	
	if (NULL == avp) {
		return NULL;
	}

	avp_header = getAvpHeader(avp);
	model = getAVP_Model(avp);

	if (NULL == avp_header) {
		fd_log_notice("avpToPyDict: Failed to get avp_header");
		return NULL;
	}
	
	py_avp_dict = PyDict_New();
	PyDict_SetItemString(py_avp_dict, "code", PyLong_FromUnsignedLong(avp_header->avp_code));
	PyDict_SetItemString(py_avp_dict, "vendor", PyLong_FromUnsignedLong(avp_header->avp_vendor));

	if (getAvpDictData(model, &avp_dict_data)) {
		PyObject* py_avp_value = getPyAvpValue(avp, avp_dict_data.avp_basetype, avp_header->avp_value);

		if (py_avp_value) {
			PyDict_SetItemString(py_avp_dict, "value", py_avp_value);
			Py_DECREF(py_avp_value);
		} else {
			fd_log_notice("avpToPyDict: failed to get py_avp_value for (%d, vendor %d)", avp_header->avp_code, avp_header->avp_vendor);
		}
	} else {
		fd_log_notice("avpToPyDict: unknown AVP (%d, vendor %d) (not in dictionary)", avp_header->avp_code, avp_header->avp_vendor);
	}

	return py_avp_dict;
}

/* Caller responsible for calling Py_XDECREF() on result if not NULL */
static PyObject* getPyAvpValue(struct avp *avp, enum dict_avp_basetype avp_type, union avp_value * avp_value) {
	PyObject* py_avp_value = NULL;

	switch (avp_type) {
		case AVP_TYPE_GROUPED: 
		{
			struct avp *firstChildAVP = getFirstAVP(avp);
			py_avp_value = avpChainToPyList(firstChildAVP);
			break;
		}
		case AVP_TYPE_OCTETSTRING:
		{
			py_avp_value = PyUnicode_FromStringAndSize((char *)avp_value->os.data, avp_value->os.len);
			break;
		}
		case AVP_TYPE_INTEGER32: 
		{
			py_avp_value = PyLong_FromLong(avp_value->i32);
			break;
		}
		case AVP_TYPE_INTEGER64: 
		{
			py_avp_value = PyLong_FromLongLong(avp_value->i64);
			break;
		}
		case AVP_TYPE_UNSIGNED32: 
		{
			py_avp_value = PyLong_FromUnsignedLong(avp_value->u32);
			break;
		}
		case AVP_TYPE_UNSIGNED64: 
		{
			py_avp_value = PyLong_FromUnsignedLongLong(avp_value->u64);
			break;
		}
		case AVP_TYPE_FLOAT32: 
		{
			py_avp_value = PyFloat_FromDouble((double)avp_value->f32);
			break;
		}
		case AVP_TYPE_FLOAT64: 
		{
			py_avp_value = PyFloat_FromDouble(avp_value->f64);
			break;
		}
		default:
		{
			fd_log_notice("getPyAvpValue: Unknown AVP value type of %i\n", avp_type);
			break;
		}
	}

	return py_avp_value;
}

/* Caller responsible for calling Py_XDECREF() on result if not NULL */
static PyObject* msgHeaderToPyDict(struct msg *msg) {
    PyObject* py_msg_header_dict = NULL;
	struct msg_hdr *msg_header = getMsgHeader(msg);

	if (msg_header) {
		PyObject* py_msg_appl = PyLong_FromUnsignedLong(msg_header->msg_appl);
		PyObject* py_msg_flags = PyLong_FromUnsignedLong(msg_header->msg_flags);
		PyObject* py_msg_code = PyLong_FromUnsignedLong(msg_header->msg_code);
		PyObject* py_msg_hbhid = PyLong_FromUnsignedLong(msg_header->msg_hbhid);
		PyObject* py_msg_eteid = PyLong_FromUnsignedLong(msg_header->msg_eteid);

		if ((NULL == py_msg_appl)  ||
		    (NULL == py_msg_flags) ||
		    (NULL == py_msg_code)  ||
		    (NULL == py_msg_hbhid) ||
		    (NULL == py_msg_eteid)) {
			fd_log_notice("msgHeaderToPyDict: Failed to create msg header PyObject\n");
		} else {
			py_msg_header_dict = PyDict_New();
			PyDict_SetItemString(py_msg_header_dict, "app_id", py_msg_appl);
			PyDict_SetItemString(py_msg_header_dict, "flags", py_msg_flags);
			PyDict_SetItemString(py_msg_header_dict, "cmd_code", py_msg_code);
			PyDict_SetItemString(py_msg_header_dict, "hbh_id", py_msg_hbhid);
			PyDict_SetItemString(py_msg_header_dict, "e2e_id", py_msg_eteid);
		}

		Py_XDECREF(py_msg_appl);
		Py_XDECREF(py_msg_flags);
		Py_XDECREF(py_msg_code);
		Py_XDECREF(py_msg_hbhid);
		Py_XDECREF(py_msg_eteid);
	} else {
		fd_log_notice("msgHeaderToPyDict: Failed to get msg header\n");
	}

	return py_msg_header_dict;
}

/* Caller responsible for calling Py_XDECREF() on result if not NULL */
static PyObject * pythonDraTransformValue(PyObject* py_msg_dict, PyObject* py_peers_dict) {
	PyObject *py_result_dict = NULL;
	PyObject *py_function = NULL;
	
	py_function = pythonDraGetFunction();
	
	if (py_function) {
		py_result_dict = PyObject_CallFunctionObjArgs(py_function, py_msg_dict, py_peers_dict, NULL);
		Py_XDECREF(py_function);
	}

    return py_result_dict;
}

/* Caller responsible for calling Py_XDECREF() on result if not NULL */
/* Assumes global state has been set */
static PyObject * pythonDraGetFunction(void) {
	PyObject *py_function = NULL;
    PyObject *py_module = pythonDraGetModule();

	if (py_module) {
		py_function = PyObject_GetAttrString(py_module, pythonDraState.functionName);
		Py_XDECREF(py_module);
	}

	return py_function;
}

/* Caller responsible for calling Py_XDECREF() on result if not NULL */
/* Assumes global state has been set */
static PyObject *pythonDraGetModule(void) {
    PyObject *py_module = NULL;
    PyObject *py_module_name = PyUnicode_FromString(pythonDraState.moduleName);

    if (py_module_name) {
        py_module = PyImport_Import(py_module_name);
        Py_XDECREF(py_module_name);
    }

    return py_module;
}

/* FreeDiameter wrappers */
static struct avp *getNextAVP(struct avp *avp) {
	struct avp *nextAVP = NULL;

	if (NULL != avp) {
		fd_msg_browse(avp, MSG_BRW_NEXT, &nextAVP, NULL);
	}

	return nextAVP;
}

static struct avp *getFirstAVP(msg_or_avp *msg) {
	struct avp *avp = NULL;

	if (NULL != msg) {
		fd_msg_browse(msg, MSG_BRW_FIRST_CHILD, &avp, NULL);
	}

	return avp;
}

static struct avp_hdr *getAvpHeader(struct avp *avp) {
	struct avp_hdr *header = NULL;

	if (NULL != avp) {
		fd_msg_avp_hdr(avp, &header);
	}

	return header;
}

static struct dict_object *getAVP_Model(struct avp *avp) {
	struct dict_object *model = NULL;

	if (NULL != avp) {
		applyDictToMsgOrAvp(avp);
		fd_msg_model(avp, &model);
	}

	return model;
}

static bool getAvpDictData(struct dict_object *model, struct dict_avp_data* avpDictData) {
	if ((NULL != model) &&
	    (NULL != avpDictData)) {
		return (0 == fd_dict_getval(model, avpDictData));
	}

	return false;
}

static struct msg_hdr* getMsgHeader(struct msg *msg) {
	struct msg_hdr *msg_header = NULL;
	
	if (NULL != msg) {
		fd_msg_hdr(msg, &msg_header);
	}

	return msg_header;
}

static struct avp* findAvp(struct avp *first_avp, uint32_t avp_code, uint32_t avp_vendor) {
	struct avp * found = NULL;

	for (struct avp *avp = first_avp; NULL != avp; avp = getNextAVP(avp)) {
		struct avp_hdr *avp_header = getAvpHeader(avp);

		if (NULL == avp_header) {
			/* Error AVP has no header? */
			fd_log_notice("findAvp: Could not get AVP header... Skipping AVP\n");
			continue;
		}

		if ((avp_code == avp_header->avp_code) &&
		    (avp_vendor == avp_header->avp_vendor)) {
			found = avp;
			break;
		}
	}
	
	return found;
}

static struct avp* newAvpInstance(uint32_t avp_code, uint32_t avp_vendor) {
	struct avp* avp = NULL;
	struct dict_object * dict_obj = NULL;
	struct dict_avp_request dict_avp_req = {
		.avp_code = avp_code,
		.avp_vendor = avp_vendor
	};

	int found = fd_dict_search(fd_g_config->cnf_dict,
			    		       DICT_AVP,
			    		       AVP_BY_CODE_AND_VENDOR,
			    		       &dict_avp_req,
			    		       &dict_obj,
			    		       ENOENT);
    if ((ENOENT == found) || 
	    (NULL == dict_obj)) {
		/* Failed to get the dict object, cannot 
		 * create new AVP without one */
		return NULL;
	}

	/* AVP remains NULL if this fails */
	fd_msg_avp_new(dict_obj, 0, &avp);

	return avp;
}

static bool setAvpValue(union avp_value* value, struct avp* avp) {
	if ((NULL != value) &&
        (NULL != avp)) {
        return 0 == fd_msg_avp_setvalue(avp, value);
    }

    return false;
}

static bool removeAvp(struct avp* avp) {
	if (NULL != avp) {
		fd_msg_unhook_avp(avp);
		int val = fd_msg_free(avp);
		fd_log_notice("fd_msg_free(avp) = %i (should == 0)", val);
		return 0 == val;
	} else {
		fd_log_error("avp was null, find avp function failed...");
	}

	return false;
}



static bool addAvp(struct avp* avp, msg_or_avp* msg_or_avp) {
    if ((NULL != avp) &&
        (NULL != msg_or_avp)) {
		if (isAvp(msg_or_avp) && 
			(AVP_TYPE_GROUPED != getAvpBasetype(msg_or_avp))) {
			/* We cannot add an AVP to a non-grouped AVP */
    		return false;
		}

        return 0 == fd_msg_avp_add(msg_or_avp, MSG_BRW_LAST_CHILD, avp);
    }

    return false;
}

/* Does some magic with the model so we can encode/deode the AVPs */
static bool applyDictToMsgOrAvp(msg_or_avp* msg_or_avp) {
	if (NULL != msg_or_avp) {
		return (0 == fd_msg_parse_dict(msg_or_avp, fd_g_config->cnf_dict, NULL));
	}

	return false;
}

static enum dict_avp_basetype getAvpBasetype(struct avp* avp) {
	enum dict_avp_basetype basetype = -1;
	struct dict_avp_data avp_dict_data = {0};
	struct dict_object *model = getAVP_Model(avp);

	if (getAvpDictData(model, &avp_dict_data)) {
		basetype = avp_dict_data.avp_basetype;
	}

	return basetype;
}

static bool isAvp(msg_or_avp* msg_or_avp) {
	if (NULL != msg_or_avp) {
		return (1 == fd_is_avp(msg_or_avp));
	}

	return false;
}