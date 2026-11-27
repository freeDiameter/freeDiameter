/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef FD_DISCOVERY_CONFIG_H
#define FD_DISCOVERY_CONFIG_H

#include <stdint.h>

int fd_disc_conf_begin(const char *realm);
int fd_disc_conf_application(const char *application);
int fd_disc_conf_peer(const char *identity);
int fd_disc_conf_family(unsigned int family);
int fd_disc_conf_transport(unsigned int transport);
int fd_disc_conf_timer(int tc, int value);
int fd_disc_conf_priority(const char *priority);
int fd_disc_conf_peer_realm(const char *realm);
int fd_disc_conf_end(void);
int fd_disc_conf_start(void);
int fd_disc_conf_fini(void);

#endif
