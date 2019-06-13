# The test_ccload extension
PROJECT("Credit Control load generator server" C)

FD_ADD_EXTENSION(test_ccload test_ccload.c)


####
## INSTALL section ##

INSTALL(TARGETS test_ccload
	LIBRARY DESTINATION ${INSTALL_EXTENSIONS_SUFFIX}
	COMPONENT freeDiameter-debug-tools)

