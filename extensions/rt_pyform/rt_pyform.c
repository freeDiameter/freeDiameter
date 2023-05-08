#include <freeDiameter/extension.h>
#include <python3.10/Python.h>
#include <stdlib.h>
#include <signal.h>
#include "rt_pyform_config.h"
#include <stdbool.h>


#define MODULE_NAME "rt_pyform"










/* TODO
	- [ ] go through the result and compare it with the original msg structure
	- [ ] if there are any differences then change them
	- [ ] also send in the diameter peers = somehow
		- [ ] name
		- [ ] realm
		- [ ] supported applications
		- [ ] priority

	
	
	
   Future
	- [ ] if there are new values then add them?
*/









// /* Do not directly interact with the fields in this struct */
typedef struct
{
    bool initialised;

    char directoryPath[MAX_DIRECTORY_PATH];
    char moduleName[MAX_MODULE_NAME];
    char functionName[MAX_FUNCTION_NAME];
} PyformerState;

/* Static objects */
static struct fd_rt_fwd_hdl *rt_pyform_handle = NULL;
static pthread_rwlock_t rt_pyform_lock;
static PyformerState pyformerState = {0};


/* Static function prototypes */
static int pyformerInitialise(
    PyformerState *const state,
    char const *const directoryPath,
    char const *const moduleName,
    char const *const functionName);
static int pyformerFinalise(PyformerState *const state);
static int pyformerSetContext(
    PyformerState *const state,
    char const *const directoryPath,
    char const *const moduleName,
    char const *const functionName);
static int rt_pyform_entry(char *conffile);
static int rt_pyform(void *cbdata, struct msg **msg);
static PyObject* msgToPyDict(struct msg *msg);
static PyObject* avpChainToPyList(struct avp *first_avp);
static PyObject* avpToPyDict(struct avp *avp);
static PyObject* getPyAvpValue(struct avp *avp, enum dict_avp_basetype avp_type, union avp_value * avp_value);
static PyObject* msgHeaderToPyDict(struct msg *msg);
static PyObject * pyformerTransformValue(PyObject* py_msg_dict);
static PyObject * pyformerGetFunction(void);
static PyObject *pyformerGetModule(void);
static struct avp *getNextAVP(struct avp *avp);
static struct avp *getFirstAVP(msg_or_avp *msg);
static struct avp_hdr *getAVP_Header(struct avp *avp);
static struct dict_object *getAVP_Model(struct avp *avp);
static struct avp * findAvp(struct avp *first_avp, uint32_t avp_code, uint32_t avp_vendor);
static struct avp * findAvp(struct avp *first_avp, uint32_t avp_code, uint32_t avp_vendor);
static struct avp* newAvpInstance(uint32_t avp_code, uint32_t avp_vendor);
static bool setAvpValue(union avp_value* value, struct avp* avp);
static bool addAvpToMsg(struct avp* avp, struct msg *msg);
static bool getAvpDictData(struct dict_object *model, struct dict_avp_data* avpDictData);
static bool removeAvp(struct avp* avp);


/* Define the entry point function */
EXTENSION_ENTRY("rt_pyform", rt_pyform_entry);


/* Cleanup the callbacks */
void fd_ext_fini(void)
{
	pyformerFinalise(&pyformerState);
}

// have the option to expose the raw data to python or we could use the dicts to correctly decode them

void debug_print(struct avp *first_avp) {
	for (struct avp *avp = first_avp; NULL != avp; avp = getNextAVP(avp))
	{
		struct avp_hdr *AVP_Header = getAVP_Header(avp);

		if (NULL == AVP_Header) {
			printf("Error: AVP has no header\n");
			continue;
		}

		printf("[debug_print] Code:%d, Vendor:%d\n", AVP_Header->avp_code, AVP_Header->avp_vendor);
		struct dict_avp_data AVP_DictData = {0};
		struct dict_object *model = getAVP_Model(avp);
		if (getAvpDictData(model, &AVP_DictData)) {
			if (AVP_TYPE_GROUPED == AVP_DictData.avp_basetype) {
				debug_print(getFirstAVP(avp));
			}
		}


	}
}




static void peers() {
	CHECK_FCT_DO( pthread_rwlock_rdlock(&fd_g_peers_rw), /* continue */ );

	struct fd_list * li;
	struct fd_list * lj;
	int count = 0;
	char * buf = NULL;
	size_t len;


	for (li = fd_g_peers.next; li != &fd_g_peers; li = li->next) {
		struct peer_hdr * p = (struct peer_hdr *)li->o;
		count++;
		printf("!peer count %i\n", count);

		printf("p->info.pi_diamid            : '%s'\n", p->info.pi_diamid);                        // name
		printf("p->info.runtime.pir_realm    : '%s'\n", p->info.runtime.pir_realm);				   // realm
		for (lj = p->info.runtime.pir_apps.next; lj != &p->info.runtime.pir_apps; lj = lj->next) { // supported_applications
			struct fd_app *a = (struct fd_app *)lj->o;
			printf("a : '%p'\n", a); // why null?
			// printf("a->appid : '%d'\n", a->appid);
			// printf("a->vndid : '%d'\n", a->vndid);
		}

		/* priority? */
		printf("p->info.config.pic_priority  : '%s'\n", p->info.config.pic_priority); 
		
		printf("p->info.runtime.pir_prodname : '%s'\n", p->info.runtime.pir_prodname);
		printf("p->info.config.pic_realm     : '%s'\n", p->info.config.pic_realm);

		printf("p->info.runtime.pir_apps.head : %p\n", p->info.runtime.pir_apps.head);
		printf("p->info.runtime.pir_apps.next : %p\n", p->info.runtime.pir_apps.next);
		printf("&p->info.runtime.pir_apps     : %p\n", &p->info.runtime.pir_apps);


		printf("%s\n", fd_peer_dump(&buf, &len, NULL, p, 1));
	}

	CHECK_FCT_DO( pthread_rwlock_unlock(&fd_g_peers_rw), /* continue */ );
}

static int pyformerInitialise(
    PyformerState *const state,
    char const *const directoryPath,
    char const *const moduleName,
    char const *const functionName)
{
    int isFailed = 0;

    if (NULL != state)
    {
        if (0 == pyformerSetContext(state, directoryPath, moduleName, functionName))
        {
            setenv("PYTHONPATH", state->directoryPath, 1);
            Py_Initialize();
            state->initialised = true;

            isFailed = 0;
        }
    }

    return isFailed;
}

static int pyformerFinalise(PyformerState *const state)
{
    int isFailed = 0;

    if (NULL != state)
    {
        if (0 != Py_FinalizeEx())
        {
            printf("Failed to finalise cpython\n");
        }
        else
        {
            state->initialised = false;
            isFailed = 0;
        }
    }

    return isFailed;
}

static int pyformerSetContext(
    PyformerState *const state,
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
static int rt_pyform_entry(char *conffile)
{
	TRACE_ENTRY("%p", conffile);
	int ret = 0;
	char directoryPath[MAX_DIRECTORY_PATH] = "";
	char moduleName[MAX_MODULE_NAME] = "";
	char functionName[MAX_FUNCTION_NAME] = "";

	pthread_rwlock_init(&rt_pyform_lock, NULL);
	if (pthread_rwlock_wrlock(&rt_pyform_lock) != 0)
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

	pyformerInitialise(&pyformerState, directoryPath, moduleName, functionName);

	if (pthread_rwlock_unlock(&rt_pyform_lock) != 0)
	{
		fd_log_notice("%s: write-unlock failed, aborting", MODULE_NAME);
		return EDEADLK;
	}

	/* Register the callback */
	if ((ret = fd_rt_fwd_register(rt_pyform, NULL, RT_FWD_ALL, &rt_pyform_handle)) != 0)
	{
		fd_log_error("Cannot register callback handler");
		return ret;
	}

	fd_log_notice("Extension 'Pyform' initialized");
	return 0;
}


// static PyObject* findPyAvpValue(PyObject* py_avp_list, uint32_t target_avp_code, uint32_t target_avp_vendor) {
// 	PyObject* py_avp_value = NULL;

// 	Py_ssize_t size = PyList_Size(py_avp_list);
// 	for (Py_ssize_t i = 0; i < size; i++) {
// 		/* Assuming list has dicts */
// 		PyObject* py_avp_dict = PyList_GetItem(py_avp_list, i);
//     	PyObject* py_avp_code = PyDict_GetItemString(py_avp_dict, "code");
//     	PyObject* py_avp_vendor = PyDict_GetItemString(py_avp_dict, "vendor");
		
// 		uint32_t avp_code = PyLong_AsUnsignedLong(py_avp_code);
// 		uint32_t avp_vendor = PyLong_AsUnsignedLong(py_avp_vendor);

//         // printf("avp_code   : %u == %u?\n", avp_code, target_avp_code);
//         // printf("avp_vendor : %u == %u?\n", avp_vendor, target_avp_vendor);
		
// 		if ((target_avp_code == avp_code) &&
// 		    (target_avp_vendor == avp_vendor)) {
// 			py_avp_value = PyDict_GetItemString(py_avp_dict, "value");
// 			// printf("found py_avp_value: %p!\n", py_avp_value);
// 			break;
// 		}
// 	}

// 	return py_avp_value;
// }

// static enum dict_avp_basetype getAvpBasetype(struct avp *avp) {
// 	enum dict_avp_basetype basetype = -1;
// 	struct dict_avp_data AVP_DictData = {0};
// 	struct dict_object *model = getAVP_Model(avp);
	
// 	if ((NULL != model) &&
// 		(0 == fd_dict_getval(model, &AVP_DictData))) {
// 		basetype = AVP_DictData.avp_basetype;
// 	}

// 	return basetype;
// }

static int updateAvpValue(PyObject* py_avp_value, struct avp *avp) {
	int errors = 0;
	struct dict_avp_data AVP_DictData = {0};
	fd_msg_parse_dict(avp, fd_g_config->cnf_dict, NULL);


	union avp_value avp_value = {0};

	struct dict_object *model = getAVP_Model(avp);
	struct avp_hdr *AVP_Header = getAVP_Header(avp);

	if (NULL == py_avp_value) {
		printf("could not get py_avp_value\n");
		return 1;
	}

	if (NULL == avp) {
		printf("could not get avp\n");
		return 1;
	}

	if (NULL == AVP_Header) {
		printf("could not get avp header\n");
		return 1;
	}

	if (!getAvpDictData(model, &AVP_DictData)) {
		printf("Error: Trying to pyform unknown AVP (not in dictionary)\n");
		return 1;
	}

	switch (AVP_DictData.avp_basetype) {
		case AVP_TYPE_GROUPED: 
		{
			/* Caller should be handling grouped */
			printf("Error: Trying to pyform grouped AVP [Code:%d, Vendor:%d]\n", AVP_Header->avp_code, AVP_Header->avp_vendor);
			errors += 1;
			break;
		}
		case AVP_TYPE_OCTETSTRING:
		{
			// todo refactor
			if (PyUnicode_Check(py_avp_value)) {
				const char* data = PyUnicode_AsUTF8(py_avp_value);

				if (NULL != data) {
					avp_value.os.data = (uint8_t*)data;
					avp_value.os.len = strlen(data);
				}
			} else {
				printf("error expected integer value for AVP [Code: %i, Vendor: %i]\n", AVP_Header->avp_code, AVP_Header->avp_vendor);
				errors += 1;
			}
			break;
		}
		case AVP_TYPE_INTEGER32: 
		{
			if (PyLong_Check(py_avp_value)) {
				avp_value.i32 = PyLong_AsLong(py_avp_value);
			} else {
				printf("error expected integer value for AVP [Code: %i, Vendor: %i]\n", AVP_Header->avp_code, AVP_Header->avp_vendor);
				errors += 1;
			}
			break;
		}
		case AVP_TYPE_INTEGER64: 
		{
			if (PyLong_Check(py_avp_value)) {
				avp_value.i64 = PyLong_AsLongLong(py_avp_value);
			} else {
				printf("error expected integer value for AVP [Code: %i, Vendor: %i]\n", AVP_Header->avp_code, AVP_Header->avp_vendor);
				errors += 1;
			}
			break;
		}
		case AVP_TYPE_UNSIGNED32: 
		{
			if (PyLong_Check(py_avp_value)) {
				avp_value.u32 = PyLong_AsUnsignedLong(py_avp_value);
			} else {
				printf("error expected integer value for AVP [Code: %i, Vendor: %i]\n", AVP_Header->avp_code, AVP_Header->avp_vendor);
				errors += 1;
			}
			break;
		}
		case AVP_TYPE_UNSIGNED64: 
		{
			if (PyLong_Check(py_avp_value)) {
				avp_value.u64 = PyLong_AsUnsignedLongLong(py_avp_value);
			} else {
				printf("error expected integer value for AVP [Code: %i, Vendor: %i]\n", AVP_Header->avp_code, AVP_Header->avp_vendor);
				errors += 1;
			}
			break;
		}
		case AVP_TYPE_FLOAT32: 
		{
			if (PyFloat_Check(py_avp_value)) {
				avp_value.f32 = (float)PyFloat_AsDouble(py_avp_value);
			} else {
				printf("error expected floating point value for AVP [Code: %i, Vendor: %i]\n", AVP_Header->avp_code, AVP_Header->avp_vendor);
				errors += 1;
			}
			break;
		}
		case AVP_TYPE_FLOAT64: 
		{
			if (PyFloat_Check(py_avp_value)) {
				avp_value.f64 = PyFloat_AsDouble(py_avp_value);
			} else {
				printf("error expected floating point value for AVP [Code: %i, Vendor: %i]\n", AVP_Header->avp_code, AVP_Header->avp_vendor);
				errors += 1;
			}
			break;
		}
		default:
		{
			printf("We just got a type error for Code: %i, Vendor: %i\n", AVP_Header->avp_code, AVP_Header->avp_vendor);
			errors += 1;
			break;
		}
	}

	if (!setAvpValue(&avp_value, avp)) {
		printf("failed to set the vaac[av[avp]] %i\n", errors);
		errors += 1;
	}


	printf("errorssss %i\n", errors);

	return errors;

}

static int updateAvps(PyObject* py_update_avps_list, struct avp *first_avp) {
	int errors = 0;
	struct avp *current_avp = first_avp;
	fd_msg_parse_dict(current_avp, fd_g_config->cnf_dict, NULL);

	Py_ssize_t size = PyList_Size(py_update_avps_list);
	for (Py_ssize_t i = 0; i < size; i++) {
		/* Assuming list has dicts */
		PyObject* py_avp_dict = PyList_GetItem(py_update_avps_list, i);
    	PyObject* py_avp_code = PyDict_GetItemString(py_avp_dict, "code");
    	PyObject* py_avp_vendor = PyDict_GetItemString(py_avp_dict, "vendor");
    	PyObject* py_avp_value = PyDict_GetItemString(py_avp_dict, "value");
        /* todo check above exist */

		/* If this is a dict then we are changing the child */
		if (PyList_Check(py_avp_value)) {
			errors += updateAvps(py_avp_value, getFirstAVP(current_avp));
		} else {
			/* Search through avp chain until we find the one python referes tos */
			uint32_t avp_code = PyLong_AsUnsignedLong(py_avp_code);
			uint32_t avp_vendor = PyLong_AsUnsignedLong(py_avp_vendor);
			current_avp = findAvp(first_avp, avp_code, avp_vendor);
			if (NULL == current_avp) {
				printf("Could not find avp for change... skipping C:%i V:%i\n", avp_code, avp_vendor);
				errors += 1;
				continue;
			}
			errors += updateAvpValue(py_avp_value, current_avp);
		}
	}

	return errors;
}

static int addAvpsToMsg(PyObject* py_add_avps_list, struct avp *first_avp, struct msg *msg) {
	int errors = 0;
	fd_msg_parse_dict(first_avp, fd_g_config->cnf_dict, NULL);

	Py_ssize_t size = PyList_Size(py_add_avps_list);
	for (Py_ssize_t i = 0; i < size; i++) {
		/* Assuming list has dicts */
		PyObject* py_avp_dict = PyList_GetItem(py_add_avps_list, i);
    	PyObject* py_avp_code = PyDict_GetItemString(py_avp_dict, "code");
    	PyObject* py_avp_vendor = PyDict_GetItemString(py_avp_dict, "vendor");
    	PyObject* py_avp_value = PyDict_GetItemString(py_avp_dict, "value");

		if ((NULL == py_avp_dict)   ||
		    (NULL == py_avp_code)   ||
		    (NULL == py_avp_vendor) ||
		    (NULL == py_avp_value)) {
			printf("something went bad...\n");
            continue; // for now only support addint top level avps
		}

		/* If this is a dict then we are adding a new child */
		if (PyList_Check(py_avp_value)) {
            continue; // for now only support addint top level avps
			// errors += updateAvps(py_avp_value, getFirstAVP(current_avp));
		} else {
			// errors += updateAvpValue(py_avp_value, current_avp);
		}

        /* todo check above exist */
		uint32_t avp_code = PyLong_AsUnsignedLong(py_avp_code);
		uint32_t avp_vendor = PyLong_AsUnsignedLong(py_avp_vendor);
        printf("\n\ntrying to add avp_code : %i, avp_vendor : %i\n", avp_code, avp_vendor);

        struct avp* avp = newAvpInstance(avp_code, avp_vendor);
        errors += updateAvpValue(py_avp_value, avp);
        if (!addAvpToMsg(avp, msg)) {
            errors += 1;
        }
	}

    return errors;
}

static int removeAvpsFromMsg(PyObject* py_add_avps_list, struct avp *first_avp) {
    int errors = 0;

	Py_ssize_t size = PyList_Size(py_add_avps_list);
	printf("Py_ssize_t size is %li \n", size);

	for (Py_ssize_t i = 0; i < size; i++) {
		/* Assuming list has dicts */
		PyObject* py_avp_dict = PyList_GetItem(py_add_avps_list, i);
    	PyObject* py_avp_code = PyDict_GetItemString(py_avp_dict, "code");
    	PyObject* py_avp_vendor = PyDict_GetItemString(py_avp_dict, "vendor");
    	PyObject* py_avp_value = PyDict_GetItemString(py_avp_dict, "value");

		if ((NULL == py_avp_dict)   ||
		    (NULL == py_avp_code)   ||
		    (NULL == py_avp_vendor) ||
		    (NULL == py_avp_value)) {
			printf("something went bad...\n");
            continue; // for now only support addint top level avps
		}

		uint32_t avp_code = PyLong_AsUnsignedLong(py_avp_code);
		uint32_t avp_vendor = PyLong_AsUnsignedLong(py_avp_vendor);
		struct avp * avp = findAvp(first_avp, avp_code, avp_vendor);

		/* If this is a list with children then we are removing a child */
		if ((PyList_Check(py_avp_value)) && 
		    (0 < PyList_Size(py_avp_value))) {
			printf("trtying to remove avp with child c: %i, v: %i\n", avp_code, avp_vendor);
			errors += removeAvpsFromMsg(py_avp_value, getFirstAVP(avp));
		} else {
			if (avp == first_avp) {
				first_avp = getNextAVP(first_avp);
			}

			if (!removeAvp(avp)) {
				printf("failed to remove avp c: %i, v: %i\n", avp_code, avp_vendor);
			} else {
				printf("successful removal of avp c: %i, v: %i\n", avp_code, avp_vendor);
			}
		}
	}


    return errors;
}


static bool mutateMsg(PyObject* py_result_dict, struct msg *msg) {
	bool isSuccess = false;
	int errors = 0;
	struct avp *first_avp = getFirstAVP(msg);
	fd_msg_parse_dict(first_avp, fd_g_config->cnf_dict, NULL);

	if ((NULL == py_result_dict) ||
	    (NULL == msg)            ||
		(NULL == first_avp))
	{
		printf("Parameter error\n");
		return false;
	}

	PyObject* py_add_avps_list = PyDict_GetItemString(py_result_dict, "add_avps");
	PyObject* py_update_avps_list = PyDict_GetItemString(py_result_dict, "update_avps");
	PyObject* py_remove_avps_list = PyDict_GetItemString(py_result_dict, "remove_avps");

	if (py_update_avps_list) {
		/* cant change value */
		errors = 0;
		errors += updateAvps(py_update_avps_list, first_avp);
		printf("got %i errors from updating avps\n", errors);
		Py_DECREF(py_update_avps_list);
	}

	if (py_add_avps_list) {
		errors = 0;
		errors += addAvpsToMsg(py_add_avps_list, first_avp, msg);
		printf("got %i errors from adding avps\n", errors);
		Py_DECREF(py_add_avps_list);
	}

	if (py_remove_avps_list) {
		errors = 0;
		errors += removeAvpsFromMsg(py_remove_avps_list, first_avp);
		printf("got %i errors from remving avps\n", errors);
		Py_DECREF(py_remove_avps_list);
	}

	Py_DECREF(py_result_dict);

	if (0 == errors) {
		isSuccess = true;
	}

	return isSuccess;
}

static int rt_pyform(void *cbdata, struct msg **msg)
{
	PyObject* py_result_dict = NULL;
	PyObject* py_msg_dict = NULL;

	if (!pyformerState.initialised) {
		fd_log_error("rt_pyform called with uninitialised state, returning error", MODULE_NAME);
		return errno;
	}

	if (pthread_rwlock_wrlock(&rt_pyform_lock) != 0)
	{
		fd_log_error("%s: locking failed, aborting message pyform", MODULE_NAME);
		return errno;
	}

	if (*msg) {
		py_msg_dict = msgToPyDict(*msg);
	}

	if (py_msg_dict) {
		py_result_dict = pyformerTransformValue(py_msg_dict);
		Py_XDECREF(py_msg_dict);
	}

	if (py_result_dict) {
		mutateMsg(py_result_dict, *msg);
		Py_XDECREF(py_result_dict);
	}
	
	struct avp* avp = getFirstAVP(*msg);
	debug_print(avp);

	if (0) {
		peers();
	}

	if (pthread_rwlock_unlock(&rt_pyform_lock) != 0)
	{
		fd_log_error("%s: unlocking failed, returning error", MODULE_NAME);
		return errno;
	}

	return 0;
}

/* Caller responsible for calling Py_XDECREF() on result if not NULL */
static PyObject* msgToPyDict(struct msg *msg)
{
	PyObject* py_msg_dict = msgHeaderToPyDict(msg);

	if (py_msg_dict) {
		struct avp *first_avp = getFirstAVP(msg);
		PyObject* py_avp_list = avpChainToPyList(first_avp);

		if (py_avp_list) {
			PyDict_SetItemString(py_msg_dict, "avp_list", py_avp_list);
			Py_XDECREF(py_avp_list);
		}
	}

	return py_msg_dict;
}

static PyObject* avpChainToPyList(struct avp *first_avp) {
	PyObject* py_avp_list = PyList_New(0);

	for (struct avp *avp = first_avp; NULL != avp; avp = getNextAVP(avp))
	{
		PyObject* py_avp_dict = avpToPyDict(avp);
		PyList_Append(py_avp_list, py_avp_dict);
	}

	return py_avp_list;
}

static PyObject* avpToPyDict(struct avp *avp) {
	fd_msg_parse_dict(avp, fd_g_config->cnf_dict, NULL);
	PyObject* py_avp_dict = PyDict_New();
	struct avp_hdr *AVP_Header = getAVP_Header(avp);
	struct dict_avp_data AVP_DictData = {0};
	struct dict_object *model = getAVP_Model(avp);
	
	PyDict_SetItemString(py_avp_dict, "code", PyLong_FromUnsignedLong(AVP_Header->avp_code));
	PyDict_SetItemString(py_avp_dict, "vendor", PyLong_FromUnsignedLong(AVP_Header->avp_vendor));

	if ((NULL != model) &&
	    (0 == fd_dict_getval(model, &AVP_DictData))) {
		PyObject* py_avp_value = getPyAvpValue(avp, AVP_DictData.avp_basetype, AVP_Header->avp_value);
		PyDict_SetItemString(py_avp_dict, "value", py_avp_value);
	} else {
		fd_log_notice("unknown AVP (%d, vendor %d) (not in dictionary)", AVP_Header->avp_code, AVP_Header->avp_vendor);
		PyDict_SetItemString(py_avp_dict, "value", PyUnicode_FromString("unknown avp"));
	}

	return py_avp_dict;
}

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
			py_avp_value = PyUnicode_FromString("avp type error");
			printf("We just got a type error of %i\n", avp_type);
			break;
		}
	}

	return py_avp_value;
}

/* Caller responsible for calling Py_XDECREF() on result if not NULL */
static PyObject* msgHeaderToPyDict(struct msg *msg) {
	struct msg_hdr *msgHeader = NULL;
    PyObject* py_msg_header_dict = NULL;

	if (0 == fd_msg_hdr(msg, &msgHeader)) 
	{
		PyObject* py_msg_appl = PyLong_FromUnsignedLong(msgHeader->msg_appl);
		PyObject* py_msg_flags = PyLong_FromUnsignedLong(msgHeader->msg_flags);
		PyObject* py_msg_code = PyLong_FromUnsignedLong(msgHeader->msg_code);
		PyObject* py_msg_hbhid = PyLong_FromUnsignedLong(msgHeader->msg_hbhid);
		PyObject* py_msg_eteid = PyLong_FromUnsignedLong(msgHeader->msg_eteid);

		py_msg_header_dict = PyDict_New();
		PyDict_SetItemString(py_msg_header_dict, "app_id", py_msg_appl);
		PyDict_SetItemString(py_msg_header_dict, "flags", py_msg_flags);
		PyDict_SetItemString(py_msg_header_dict, "cmd_code", py_msg_code);
		PyDict_SetItemString(py_msg_header_dict, "hbh_id", py_msg_hbhid);
		PyDict_SetItemString(py_msg_header_dict, "e2e_id", py_msg_eteid);

		Py_XDECREF(py_msg_appl);
		Py_XDECREF(py_msg_flags);
		Py_XDECREF(py_msg_code);
		Py_XDECREF(py_msg_hbhid);
		Py_XDECREF(py_msg_eteid);
	}

	return py_msg_header_dict;
}

/* Caller responsible for calling Py_XDECREF() on result if not NULL */
static PyObject * pyformerTransformValue(PyObject* py_msg_dict)
{
	PyObject *py_result_dict = NULL;
	PyObject *py_function = pyformerGetFunction();
	
	if (py_function) {
		py_result_dict = PyObject_CallFunctionObjArgs(py_function, py_msg_dict, NULL);
		Py_XDECREF(py_function);
	}

    return py_result_dict;
}

/* Caller responsible for calling Py_XDECREF() on result if not NULL */
/* Assumes global state has been set */
static PyObject * pyformerGetFunction(void) {
	PyObject *py_function = NULL;
    PyObject *py_module = pyformerGetModule();

	if (py_module) {
		py_function = PyObject_GetAttrString(py_module, pyformerState.functionName);
		Py_XDECREF(py_module);
	}

	return py_function;
}

/* Caller responsible for calling Py_XDECREF() on result if not NULL */
/* Assumes global state has been set */
static PyObject *pyformerGetModule(void)
{
    PyObject *py_module = NULL;
    PyObject *py_module_name = PyUnicode_FromString(pyformerState.moduleName);

    if (py_module_name)
    {
        py_module = PyImport_Import(py_module_name);
        Py_XDECREF(py_module_name);
    }

    return py_module;
}


/* FreeDiameter wrappers */
static struct avp *getNextAVP(struct avp *avp)
{
	struct avp *nextAVP = NULL;

	fd_msg_browse(avp, MSG_BRW_NEXT, &nextAVP, NULL);

	return nextAVP;
}

static struct avp *getFirstAVP(msg_or_avp *msg)
{
	struct avp *avp = NULL;

	if (0 != fd_msg_browse(msg, MSG_BRW_FIRST_CHILD, &avp, NULL))
	{
		fd_log_notice("internal error: message has no child");
		pthread_rwlock_unlock(&rt_pyform_lock);
	}

	return avp;
}

static struct avp_hdr *getAVP_Header(struct avp *avp)
{
	struct avp_hdr *header = NULL;

	if (NULL == avp)
	{
		return NULL;
	}

	if (0 != fd_msg_avp_hdr(avp, &header))
	{
		fd_log_notice("internal error: unable to get header for AVP");
	}

	return header;
}

static struct dict_object *getAVP_Model(struct avp *avp)
{
	struct dict_object *model = NULL;

	if (NULL == avp)
	{
		return NULL;
	}

	if (0 != fd_msg_model(avp, &model))
	{
		fd_log_notice("internal error: unable to get model for AVP");
	}

	return model;
}

static bool getAvpDictData(struct dict_object *model, struct dict_avp_data* avpDictData)
{
	if ((NULL == model) ||
	    (NULL == avpDictData))
	{
		return false;
	}

	if (0 != fd_dict_getval(model, avpDictData))
	{
		fd_log_notice("internal error: unable to get model for AVP");
		return false;

	}

	return true;
}

static struct avp * findAvp(struct avp *first_avp, uint32_t avp_code, uint32_t avp_vendor) {
	struct avp * found = NULL;

	for (struct avp *avp = first_avp; NULL != avp; avp = getNextAVP(avp))
	{
		struct avp_hdr *AVP_Header = getAVP_Header(avp);

		if (NULL == AVP_Header) {
			printf("Error: AVP has no header\n");
			continue;
		}

		if ((avp_code == AVP_Header->avp_code) &&
		    (avp_vendor == AVP_Header->avp_vendor)) {
			found = avp;
			break;
		}
	}
	
	return found;
}

static struct avp* newAvpInstance(uint32_t avp_code, uint32_t avp_vendor) {
	struct avp* avp = NULL;
	struct dict_object * dict_obj = NULL;
	struct dict_avp_request dar = {
		.avp_code = avp_code,
		.avp_vendor = avp_vendor
	};

	int found = fd_dict_search(fd_g_config->cnf_dict,
			    		       DICT_AVP,
			    		       AVP_BY_CODE_AND_VENDOR,
			    		       &dar,
			    		       &dict_obj,
			    		       ENOENT);

    if ((ENOENT == found) || 
	    (NULL == dict_obj)) {
        printf("Error when trying to find AVP Dictionary object, unknown AVP [Code: %i, Vendor: %i]\n", avp_code, avp_vendor);
    } else if (0 != fd_msg_avp_new(dict_obj, 0, &avp)) {
        printf("Error when creating new AVP instance [Code: %i, Vendor: %i]\n", avp_code, avp_vendor);
    }

	return avp;
}

static bool setAvpValue(union avp_value* value, struct avp* avp) {
	if ((NULL != value) &&
        (NULL != avp)) {
        return 0 == fd_msg_avp_setvalue(avp, value);
    }

    printf("Failed to set AVP value\n");
    return false;
}

static bool removeAvp(struct avp* avp) {

	if (NULL == avp) {
		return false;
	}

	fd_msg_unhook_avp(avp);
	return 0 == fd_msg_free(avp);
}

static bool addAvpToMsg(struct avp* avp, struct msg* msg) {
    if ((NULL != avp) &&
        (NULL != msg)) {
        return 0 == fd_msg_avp_add(msg, MSG_BRW_LAST_CHILD, avp);
    }

    printf("Failed to add AVP to message\n");
    return false;
}
