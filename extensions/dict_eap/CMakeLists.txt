# The dict_nasreq extension
PROJECT("Diameter EAP (RFC4072) dictionary definitions" C)

# Compile as a module
FD_ADD_EXTENSION(dict_eap dict_eap.c)


####
## INSTALL section ##

INSTALL(TARGETS dict_eap
	LIBRARY DESTINATION ${INSTALL_EXTENSIONS_PATH}
	COMPONENT freeDiameter-dictionary-RFC4072)
