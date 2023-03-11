# Debug Peers Extension
PROJECT("Debug Peers Extension" C)
FD_ADD_EXTENSION(dbg_peers dbg_peers.c)


####
## INSTALL section ##

INSTALL(TARGETS dbg_peers
	LIBRARY DESTINATION ${INSTALL_EXTENSIONS_SUFFIX}
	COMPONENT freeDiameter-debug-tools)

