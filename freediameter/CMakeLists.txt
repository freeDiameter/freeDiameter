# The subproject name
Project("freediameterd" C)

SET(PROJECT_VERSION 0.1)
SET(PROJECT_COPYRIGHT "Copyright (c) 2008-2009, WIDE Project (www.wide.ad.jp) and NICT (www.nict.go.jp)")

# List of source files
SET(FD_COMMON_SRC
	fd.h
	dict_base_proto.c
	)

# Building the executable
ADD_EXECUTABLE(freediameterd ${FD_COMMON_SRC} main.c)

# The link command
LINK_DIRECTORIES(${CURRENT_BINARY_DIR}/../libfreediameter)
TARGET_LINK_LIBRARIES(freediameterd libfreediameter ${FD_LIBS})

# The unary tests directory
OPTION(SKIP_TESTS "Skip compilation of the tests?" OFF)
IF ( NOT SKIP_TESTS )
	SUBDIRS(tests)
ENDIF ( NOT SKIP_TESTS )
