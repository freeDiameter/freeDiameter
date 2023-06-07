#ifndef RT_PYFORMER_GUARD
#define RT_PYFORMER_GUARD

#include <stdbool.h>
#include <stdint.h>

enum
{
    MAX_DIRECTORY_PATH = 32,
    MAX_MODULE_NAME = 32,
    MAX_FUNCTION_NAME = 32,
};

/* Do not directly interact with the fields in this struct */
typedef struct
{
    bool initialised;

    char directoryPath[MAX_DIRECTORY_PATH];
    char moduleName[MAX_MODULE_NAME];
    char functionName[MAX_FUNCTION_NAME];
} PyformerState;

typedef struct
{
    uint32_t appID;
    uint8_t flags;
    uint32_t cmdCode;
    uint32_t HBH_ID;
    uint32_t E2E_ID;
    uint32_t AVP_Code;
    uint32_t vendorID;
    char *const value;
    unsigned int maxValueSz;
} PyformerArgs;

int pyformerInitialise(
    PyformerState *const state,
    char const *const directoryPath,
    char const *const moduleName,
    char const *const functionName);
int pyformerFinalise(PyformerState *const state);
int pyformerTransformValue(PyformerState const *const state, PyformerArgs args);

#endif /* RT_PYFORMER_GUARD */