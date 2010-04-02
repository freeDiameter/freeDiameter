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

# Require GNU TLS for building the daemon
FIND_PACKAGE(GNUTLS REQUIRED)
INCLUDE_DIRECTORIES(${GNUTLS_INCLUDE_DIRS})
SET(FD_LIBS ${FD_LIBS} ${GNUTLS_LIBRARIES})
find_path(GCRYPT_INCLUDE_DIR NAMES gcrypt.h)
If ( NOT GCRYPT_INCLUDE_DIR )
	MESSAGE(SEND_ERROR "Unable to find gcrypt.h, please install libgcrypt-dev or equivalent")
Endif ( NOT GCRYPT_INCLUDE_DIR )
INCLUDE_DIRECTORIES(${GCRYPT_INCLUDE_DIR})

# Building the executable
ADD_EXECUTABLE(freeDiameterd ${FD_COMMON_SRC} ${FD_COMMON_GEN_SRC} main.c)

# Add this to workaround an apparent bug in FreeBSD cmake (already defined in Linux)
SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "-rdynamic")

# The link command
LINK_DIRECTORIES(${CURRENT_BINARY_DIR}/../libfreeDiameter)
TARGET_LINK_LIBRARIES(freeDiameterd libfreeDiameter ${FD_LIBS})

# Save the list of files, if needed
SET(FD_COMMON_SRC ${FD_COMMON_SRC} PARENT_SCOPE)
SET(FD_COMMON_GEN_SRC ${FD_COMMON_GEN_SRC} PARENT_SCOPE)

# The unary tests directory
OPTION(SKIP_TESTS "Skip compilation of the tests?" OFF)
IF ( NOT SKIP_TESTS )
	SUBDIRS(tests)
ENDIF ( NOT SKIP_TESTS )
