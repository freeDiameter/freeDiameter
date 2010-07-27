# The dict_nasreq extension
PROJECT("Diameter SIP Application (RFC4740)" C)

FIND_PACKAGE(MySQL REQUIRED)
INCLUDE_DIRECTORIES(${MySQL_INCLUDE_DIR}) 

# Parser files
BISON_FILE(diamsip.y)
FLEX_FILE(diamsip.l)
SET_SOURCE_FILES_PROPERTIES(lex.diamsip.c diamsip.tab.c PROPERTIES COMPILE_FLAGS "-I ${CMAKE_CURRENT_SOURCE_DIR}")

# List of source files
SET( DIAM_SIP_SRC
	lex.diamsip.c
	diamsip.tab.c
	diamsip.tab.h
	diamsip.c
	diamsip.h
	libdiamsip.c
	md5.c
	multimediaauth.c
	registrationtermination.c
	userauthorization.c
	pushprofile.c
	serverassignment.c
	locationinfo.c
	locationinfosl.c
)

# Compile as a module
FD_ADD_EXTENSION(app_sip ${DIAM_SIP_SRC})

TARGET_LINK_LIBRARIES(app_sip ${MySQL_LIBRARIES})

####
## INSTALL section ##

INSTALL(TARGETS app_sip
	LIBRARY DESTINATION ${INSTALL_EXTENSIONS_SUFFIX}
	COMPONENT freeDiameter-sip-server)
