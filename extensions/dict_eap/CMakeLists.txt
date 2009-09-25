# The dict_nasreq extension
PROJECT("Diameter EAP (RFC4072) dictionary definitions" C)

# Compile as a module
FD_ADD_EXTENSION(dict_eap dict_eap.c)
