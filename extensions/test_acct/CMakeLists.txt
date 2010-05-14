# The test_acct extension
PROJECT("Accounting dummy server" C)

FD_ADD_EXTENSION(test_acct test_acct.c)


####
## INSTALL section ##

INSTALL(TARGETS test_acct
	LIBRARY DESTINATION ${DEFAULT_EXTENSIONS_PATH}
	COMPONENT freeDiameter-debug-tools)

