#include "rt_pyform_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parseConfigLine(
	char *line,
	char *const directoryPath,
	char *const moduleName,
	char *const functionName);
static int getConfigValueFromLine(char *line, char *value, unsigned int maxValueSz);

int parseConfig(
	char const *const filename,
	char *const directoryPath,
	char *const moduleName,
	char *const functionName)
{
	int numErrors = 0;
	FILE *fptr = NULL;

	if (NULL != filename)
	{
		fptr = fopen(filename, "r");
	}

	if (NULL != fptr)
	{
		char *line = NULL;
		size_t len = 0;

		while (-1 != getline(&line, &len, fptr))
		{
			numErrors += parseConfigLine(line, directoryPath, moduleName, functionName);
		}

		fclose(fptr);
	}
	else
	{
		++numErrors;
	}

	return numErrors;
}

static int parseConfigLine(
	char *line,
	char *const directoryPath,
	char *const moduleName,
	char *const functionName)
{
	int error = 0;

	if (NULL != strstr(line, "DirectoryPath"))
	{
		error = getConfigValueFromLine(line, directoryPath, MAX_DIRECTORY_PATH);
	}
	else if (NULL != strstr(line, "ModuleName"))
	{
		error = getConfigValueFromLine(line, moduleName, MAX_DIRECTORY_PATH);
	}
	else if (NULL != strstr(line, "FunctionName"))
	{
		error = getConfigValueFromLine(line, functionName, MAX_DIRECTORY_PATH);
	}
	else
	{
		error = 1;
	}

	return error;
}

static int getConfigValueFromLine(char *line, char *value, unsigned int maxValueSz)
{
	int isFailed = 1;

	char *valStart = strchr(line, '"');
	char *valEnd = strrchr(line, '"');

	if ((NULL != valStart) &&
		(NULL != valEnd) &&
		(valStart != valEnd))
	{
		/* Start is first char after " */
		++valStart;
		int valueSz = valEnd - valStart;

		if (valueSz < maxValueSz)
		{
			strncpy(value, valStart, valueSz);
			isFailed = 0;
		}
	}

	return isFailed;
}