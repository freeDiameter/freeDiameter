# The subproject name
Project("freeDiameterd" C)

# Configuration parser
BISON_FILE(fdd.y)
FLEX_FILE(fdd.l)
SET_SOURCE_FILES_PROPERTIES(lex.fdd.c fdd.tab.c PROPERTIES COMPILE_FLAGS "-I ${CMAKE_CURRENT_SOURCE_DIR}")

# List of source files
SET(FD_COMMON_SRC
	fD.h
	apps.c
	cnxctx.h
	config.c
	cnxctx.c
	endpoints.c
	events.c
	extensions.c
	dict_base_proto.c
	messages.c
	queues.c
	peers.c
	p_ce.c
	p_cnx.c
	p_dw.c
	p_dp.c
	p_expiry.c
	p_out.c
	p_psm.c
	p_sr.c
	routing_dispatch.c
	server.c
	tcp.c
	)

IF(NOT DISABLE_SCTP)
	SET(FD_COMMON_SRC ${FD_COMMON_SRC} sctp.c sctps.c)
ENDIF(NOT DISABLE_SCTP)

SET(FD_COMMON_GEN_SRC
		lex.fdd.c
		fdd.tab.c
		fdd.tab.h
	)
	
# Save the list of files for the tests 
SET(FD_COMMON_SRC ${FD_COMMON_SRC} PARENT_SCOPE)
SET(FD_COMMON_GEN_SRC ${FD_COMMON_GEN_SRC} PARENT_SCOPE)


# Require GNU TLS for building the daemon
FIND_PACKAGE(GnuTLS REQUIRED)
INCLUDE_DIRECTORIES(${GNUTLS_INCLUDE_DIR})
SET(FD_LIBS ${FD_LIBS} ${GNUTLS_LIBRARIES})
find_path(GCRYPT_INCLUDE_DIR NAMES gcrypt.h)
If ( NOT GCRYPT_INCLUDE_DIR )
	MESSAGE(SEND_ERROR "Unable to find gcrypt.h, please install libgcrypt-dev or equivalent")
Endif ( NOT GCRYPT_INCLUDE_DIR )
MARK_AS_ADVANCED(GCRYPT_INCLUDE_DIR)
INCLUDE_DIRECTORIES(${GCRYPT_INCLUDE_DIR})

# Also we need libgcrypt to... display its version :(
find_library(GCRYPT_LIBRARY 
  NAMES gcrypt
)
If ( NOT GCRYPT_LIBRARY )
	MESSAGE(SEND_ERROR "Unable to find libgcrypt, please install libgcrypt or equivalent")
Endif ( NOT GCRYPT_LIBRARY )
SET(FD_LIBS ${FD_LIBS} ${GCRYPT_LIBRARY})



# Build the executable
ADD_EXECUTABLE(freeDiameterd ${FD_COMMON_SRC} ${FD_COMMON_GEN_SRC} main.c)

# The version
SET_TARGET_PROPERTIES(freeDiameterd PROPERTIES 
	VERSION ${FD_PROJECT_VERSION_MAJOR}.${FD_PROJECT_VERSION_MINOR}.${FD_PROJECT_VERSION_REV})

# The link command
LINK_DIRECTORIES(${CURRENT_BINARY_DIR}/../libfreeDiameter)
TARGET_LINK_LIBRARIES(freeDiameterd libfreeDiameter ${FD_LIBS})

# The unary tests directory
IF ( BUILD_TESTING )
	SUBDIRS(tests)
ENDIF ( BUILD_TESTING )

####
## INSTALL section ##

INSTALL(TARGETS freeDiameterd
	RUNTIME DESTINATION ${INSTALL_DAEMON_SUFFIX}
	COMPONENT freeDiameter-daemon)
