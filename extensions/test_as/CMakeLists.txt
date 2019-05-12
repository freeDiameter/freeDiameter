# The test_as extension
PROJECT("Abort Session dummy server" C)

FD_ADD_EXTENSION(test_as test_as.c)


####
## INSTALL section ##

INSTALL(TARGETS test_as
	LIBRARY DESTINATION ${INSTALL_EXTENSIONS_SUFFIX}
	COMPONENT freeDiameter-debug-tools)

