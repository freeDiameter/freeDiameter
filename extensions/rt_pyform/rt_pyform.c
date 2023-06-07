#include <freeDiameter/extension.h>
#include <stdlib.h>
#include <signal.h>
#include "pyformer.h"
#include "rt_pyform_config.h"

#define MODULE_NAME "rt_pyform"

enum
{
	MAX_STR_LEN = 64
};

/* Static objects */
static struct fd_rt_fwd_hdl *rt_pyform_handle = NULL;
static pthread_rwlock_t rt_pyform_lock;
static PyformerState pyformerState = {0};

/* Static function prototypes */
static int rt_pyform_entry(char *conffile);
static int rt_pyform(void *cbdata, struct msg **msg);
static struct avp *getNextAVP(struct avp *avp);
static struct avp *getFirstAVP(msg_or_avp *msg);
static struct avp_hdr *getAVP_Header(struct avp *avp);
static struct dict_object *getAVP_Model(struct avp *avp);
static int getTransformedDataValue(
	struct msg_hdr *msgHeader,
	struct avp_hdr const *const header,
	char *transformedDataValue,
	unsigned int maxTransformedDataValueSz);
static int updateAVP_Value(
	struct avp *avp,
	char *transformedDataValue);
static void transformAVP(struct msg_hdr *msgHeader, struct avp *avp);
static void transformAVP_Chain(struct msg_hdr *msgHeader, struct avp *firstAVP);

/* Define the entry point function */
EXTENSION_ENTRY("rt_pyform", rt_pyform_entry);

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
		fd_log_notice("%s: Encountered errors when parning config file", MODULE_NAME);
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

static int rt_pyform(void *cbdata, struct msg **msg)
{
	struct msg_hdr *msgHeader = NULL;
	struct avp *firstAVP = getFirstAVP(*msg);

	if (pthread_rwlock_wrlock(&rt_pyform_lock) != 0)
	{
		fd_log_error("%s: locking failed, aborting message pyform", MODULE_NAME);
		return errno;
	}

	fd_msg_hdr(*msg, &msgHeader);
	transformAVP_Chain(msgHeader, firstAVP);

	if (pthread_rwlock_unlock(&rt_pyform_lock) != 0)
	{
		fd_log_error("%s: unlocking failed, returning error", MODULE_NAME);
		return errno;
	}

	return 0;
}

/* Cleanup the callbacks */
void fd_ext_fini(void)
{
	pyformerFinalise(&pyformerState);
}

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

	if (fd_msg_avp_hdr(avp, &header) != 0)
	{
		fd_log_notice("internal error: unable to get header for AVP");
	}

	return header;
}

static struct dict_object *getAVP_Model(struct avp *avp)
{
	struct dict_object *model = NULL;

	if (0 != fd_msg_model(avp, &model))
	{
		fd_log_notice("internal error: unable to get model for AVP");
	}

	return model;
}

static int getTransformedDataValue(
	struct msg_hdr *msgHeader,
	struct avp_hdr const *const header,
	char *transformedDataValue,
	unsigned int maxTransformedDataValueSz)
{
	int isFailed = 1;

	if ((NULL != msgHeader) &&
		(NULL != header->avp_value))
	{
		PyformerArgs args =
			{
				.appID = msgHeader->msg_appl,
				.flags = msgHeader->msg_flags,
				.cmdCode = msgHeader->msg_code,
				.HBH_ID = msgHeader->msg_hbhid,
				.E2E_ID = msgHeader->msg_eteid,
				.AVP_Code = header->avp_code,
				.vendorID = header->avp_vendor,
				.value = transformedDataValue,
				.maxValueSz = maxTransformedDataValueSz};

		strncpy(transformedDataValue, (char *)header->avp_value->os.data, maxTransformedDataValueSz);

		isFailed = pyformerTransformValue(&pyformerState, args);
	}

	return isFailed;
}

static int updateAVP_Value(
	struct avp *avp,
	char *transformedDataValue)
{
	int retCode = 1;
	union avp_value newAVP_Value = {0};

	newAVP_Value.os.data = (uint8_t *)transformedDataValue;
	newAVP_Value.os.len = strlen(transformedDataValue);

	retCode = fd_msg_avp_setvalue(avp, &newAVP_Value);

	return retCode;
}

static void transformAVP(struct msg_hdr *msgHeader, struct avp *avp)
{
	int retCode = 0;
	struct dict_avp_data AVP_DictData = {0};
	char transformedDataValue[MAX_STR_LEN] = "";

	fd_msg_parse_dict(avp, fd_g_config->cnf_dict, NULL);
	struct avp_hdr *AVP_Header = getAVP_Header(avp);
	struct dict_object *model = getAVP_Model(avp);

	if (NULL == AVP_Header)
	{
		fd_log_notice("internal error: unable to get header for AVP");
		return;
	}

	if (NULL == model)
	{
		fd_log_notice("unknown AVP (%d, vendor %d) (not in dictionary), skipping", AVP_Header->avp_code, AVP_Header->avp_vendor);
		return;
	}

	if (0 != fd_dict_getval(model, &AVP_DictData))
	{
		fd_log_notice("internal error: unable to get dictionary contents of AVP (%d, vendor %d)", AVP_Header->avp_code, AVP_Header->avp_vendor);
		return;
	}

	if (AVP_TYPE_GROUPED == AVP_DictData.avp_basetype)
	{
		struct avp *firstChildAVP = getFirstAVP(avp);
		transformAVP_Chain(msgHeader, firstChildAVP);
		return;
	}

	if (AVP_TYPE_OCTETSTRING != AVP_DictData.avp_basetype)
	{
		fd_log_notice("AVP (%d, vendor %d) basetype not supported (i.e. not AVP_TYPE_OCTETSTRING), skipping", AVP_Header->avp_code, AVP_Header->avp_vendor);
		return;
	}

	if (0 != getTransformedDataValue(msgHeader, AVP_Header, transformedDataValue, MAX_STR_LEN))
	{
		fd_log_notice("internal error: failed to apply transform to AVP (%d, vendor %d)", AVP_Header->avp_code, AVP_Header->avp_vendor);
		return;
	}

	retCode = updateAVP_Value(avp, transformedDataValue);

	if (0 != retCode)
	{
		/* Assuming a recoverable error so keep going through the AVP chain */
		fd_log_error("internal error, cannot set value for AVP '%s': %s", AVP_DictData.avp_name, strerror(retCode));
	}
}

static void transformAVP_Chain(struct msg_hdr *msgHeader, struct avp *firstAVP)
{
	for (struct avp *avp = firstAVP; NULL != avp; avp = getNextAVP(avp))
	{
		transformAVP(msgHeader, avp);
	}
}