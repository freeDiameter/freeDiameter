#ifndef DBG_PEERS_CONFIG
#define DBG_PEERS_CONFIG

/* Returns number of errors encountered when parsing config */
int parseConfig(
	char const *const filename,
	unsigned int* updatePeriodSec,
	unsigned short* metricExportPort);

#endif /* DBG_PEERS_CONFIG */