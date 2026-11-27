#ifndef RT_DISCOVER_H
#define RT_DISCOVER_H

#include <freeDiameter/extension.h>
#include <freeDiameter/discovery.h>

struct rd_parse;

struct rd_allowed_peer {
	struct fd_list chain;
	char *identity;
};

struct rd_realm {
	struct fd_list chain;
	char *name;
	struct fd_list peers;
	struct fd_discovery_peer_options options;
	unsigned int transport;
};

int rd_config_load(const char *path, struct fd_list *realms);
void rd_config_free(struct fd_list *realms);

#endif
