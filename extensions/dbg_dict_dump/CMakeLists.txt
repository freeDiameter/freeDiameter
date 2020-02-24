# Dictionary dump extension
PROJECT("Dictionary dump extension" C)
FD_ADD_EXTENSION(dbg_dict_dump dbg_dict_dump.c)


####
## INSTALL section ##

INSTALL(TARGETS dbg_dict_dump
	LIBRARY DESTINATION ${INSTALL_EXTENSIONS_SUFFIX}
	COMPONENT freeDiameter-debug-tools)

