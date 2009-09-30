# The subproject name
Project("freeDiameterd" C)

# Configuration parser
BISON_FILE(fdd.y)
FLEX_FILE(fdd.l)
SET_SOURCE_FILES_PROPERTIES(lex.fdd.c fdd.tab.c PROPERTIES COMPILE_FLAGS "-I ${CMAKE_CURRENT_SOURCE_DIR}")

# List of source files
SET(FD_COMMON_SRC
	fD.h
	config.c
	dispatch.c
	extensions.c
	dict_base_proto.c
	messages.c
	queues.c
	peers.c
	p_expiry.c
	p_psm.c
	)

SET(FD_COMMON_GEN_SRC
		lex.fdd.c
		fdd.tab.c
		fdd.tab.h
	)


# Building the executable
ADD_EXECUTABLE(freeDiameterd ${FD_COMMON_SRC} ${FD_COMMON_GEN_SRC} main.c)

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
