#ifndef RT_PYFORM_CONFIG
#define RT_PYFORM_CONFIG

#include "pyformer.h"

/* Returns number of errors encountered when parsing config */
int parseConfig(
    char const *const filename,
    char *const directoryPath,
    char *const moduleName,
    char *const functionName);

#endif /* RT_PYFORM_CONFIG */