#ifndef RT_PYFORM_CONFIG
#define RT_PYFORM_CONFIG

enum
{
    MAX_DIRECTORY_PATH = 32,
    MAX_MODULE_NAME = 32,
    MAX_FUNCTION_NAME = 32,
};

/* Returns number of errors encountered when parsing config */
int parseConfig(
    char const *const filename,
    char *const directoryPath,
    char *const moduleName,
    char *const functionName);

#endif /* RT_PYFORM_CONFIG */