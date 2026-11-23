# The dict_rfc7944 extension
PROJECT("Diameter Routing Message Priority (RFC 7944) Dictionary Definitions" C)

# Compile as a module
FD_ADD_EXTENSION(dict_rfc7944 dict_rfc7944.c)


####
## INSTALL section ##

INSTALL(TARGETS dict_rfc7944
	LIBRARY DESTINATION ${INSTALL_EXTENSIONS_SUFFIX}
	COMPONENT freeDiameter-dictionary-RFC7944)
