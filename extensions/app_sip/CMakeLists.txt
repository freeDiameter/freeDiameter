# The dict_nasreq extension
PROJECT("Diameter SIP Application (RFC4740)" C)

FIND_PACKAGE(MYSQL REQUIRED)
INCLUDE_DIRECTORIES(${MYSQL_INCLUDE_DIR}) 

# List of source files
SET( DIAM_SIP_SRC
	diamsip.c
	diamsip.h
	libdiamsip.c
	md5.c
	multimediaauth.c
)

# Compile as a module
FD_ADD_EXTENSION(app_sip ${DIAM_SIP_SRC})

TARGET_LINK_LIBRARIES(app_sip ${MYSQL_LIBRARY})
