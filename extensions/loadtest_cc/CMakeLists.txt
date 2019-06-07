# The loadtest_cc extension
PROJECT("Credit Control load generator server" C)

FD_ADD_EXTENSION(loadtest_cc loadtest_cc.c)


####
## INSTALL section ##

INSTALL(TARGETS loadtest_cc
	LIBRARY DESTINATION ${INSTALL_EXTENSIONS_SUFFIX}
	COMPONENT freeDiameter-debug-tools)

