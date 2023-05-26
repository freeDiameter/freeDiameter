#include "dbg_peers_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parseConfigLine(
	char *line,
	unsigned int* updatePeriodSec,
	unsigned short* metricExportPort);
static int getConfigValueFromLine(char *line, int *value);

int parseConfig(
	char const *const filename,
	unsigned int* updatePeriodSec,
	unsigned short* metricExportPort)
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
			numErrors += parseConfigLine(line, updatePeriodSec, metricExportPort);
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
	unsigned int* updatePeriodSec,
	unsigned short* metricExportPort)
{
	int error = 0;

	if (NULL != strstr(line, "UpdatePeriodSec"))
	{
		int val = 0;
		error = getConfigValueFromLine(line, &val);
		*updatePeriodSec = (unsigned int)val;
	}
	else if (NULL != strstr(line, "ExportMetricsPort"))
	{
		int val = 0;
		error = getConfigValueFromLine(line, &val);
		*metricExportPort = (unsigned short)val;
	}
	else
	{
		error = 1;
	}

	return error;
}

static int getConfigValueFromLine(char *line, int* value)
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

		if (0 < valueSz)
		{
			*value = atoi(valStart);
			isFailed = 0;
		}
	}

	return isFailed;
}