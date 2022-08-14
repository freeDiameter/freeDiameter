#include <python3.10/Python.h>
#include "pyformer.h"
#include <string.h>

static int pyformerSetContext(
    PyformerState *const state,
    char const *const directoryPath,
    char const *const moduleName,
    char const *const functionName);
static PyObject *callPythonFunctionFromModule(
    char const *const moduleName,
    char const *const functionName,
    PyformerArgs args);
static PyObject *getPythonFunctionFromModule(
    const char *const moduleName,
    const char *const functionName);
static PyObject *getPythonFunctionFromModule(
    const char *const moduleName,
    const char *const functionName);
static PyObject *importPythonModule(const char *const moduleName);
static int updateValue(
    char *const value,
    unsigned int maxValSz,
    PyObject *pyResult);

int pyformerInitialise(
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

int pyformerFinalise(PyformerState *const state)
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

int pyformerTransformValue(PyformerState const *const state, PyformerArgs args)
{
    int isFailed = 1;

    if ((NULL != state) &&
        (NULL != args.value))
    {
        if (true == state->initialised)
        {
            PyObject *pyResult = NULL;

            if ((pyResult = callPythonFunctionFromModule(state->moduleName, state->functionName, args)))
            {
                isFailed = updateValue(args.value, args.maxValueSz, pyResult);
                Py_CLEAR(pyResult);
            }
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

static PyObject *callPythonFunctionFromModule(
    char const *const moduleName,
    char const *const functionName,
    PyformerArgs args)
{
    PyObject *pyFunction = NULL;
    PyObject *pyResult = NULL;

    if ((pyFunction = getPythonFunctionFromModule(moduleName, functionName)))
    {
        pyResult = PyObject_CallFunction(
            pyFunction,
            "IBIIIIIs",
            args.appID,
            args.flags,
            args.cmdCode,
            args.HBH_ID,
            args.E2E_ID,
            args.AVP_Code,
            args.vendorID,
            args.value);

        Py_CLEAR(pyFunction);
    }

    return pyResult;
}

static PyObject *getPythonFunctionFromModule(
    const char *const moduleName,
    const char *const functionName)
{
    PyObject *pyFunction = NULL;
    PyObject *pyModule = NULL;

    if ((pyModule = importPythonModule(moduleName)))
    {
        pyFunction = PyObject_GetAttrString(pyModule, functionName);
        Py_CLEAR(pyModule);
    }

    return pyFunction;
}

static PyObject *importPythonModule(const char *const moduleName)
{
    PyObject *pyModule = NULL;
    PyObject *pyModuleName = NULL;

    if ((pyModuleName = PyUnicode_FromString(moduleName)))
    {
        pyModule = PyImport_Import(pyModuleName);
        Py_CLEAR(pyModuleName);
    }

    return pyModule;
}

static int setValue(
    char *const value,
    unsigned int maxValSz,
    char const *const newVal)
{
    int isFailed = 1;
    size_t newValSz = strlen(newVal);

    if (newValSz < maxValSz)
    {
        strcpy(value, newVal);
        isFailed = 0;
    }
    else
    {
        printf("Transform returned a string larger than what we were expecting\n");
    }

    return isFailed;
}

static int updateValue(
    char *const value,
    unsigned int maxValSz,
    PyObject *pyResult)
{
    int isFailed = 1;
    char *newVal = NULL;
    PyObject *pyBytes = NULL;

    if ((pyBytes = PyUnicode_AsUTF8String(pyResult)))
    {
        newVal = PyBytes_AsString(pyBytes);
        isFailed = setValue(value, maxValSz, newVal);
        Py_CLEAR(pyBytes);
    }

    return isFailed;
}