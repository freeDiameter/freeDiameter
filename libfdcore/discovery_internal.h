#ifndef FD_DISCOVERY_INTERNAL_H
#define FD_DISCOVERY_INTERNAL_H

/* Prevent new operations and stop all workers before peer teardown.  Handles
 * remain valid until their owners call fd_discovery_stop(). */
void fd_discovery_shutdown(void);
int fd_discovery_routing_init(void);

#endif
