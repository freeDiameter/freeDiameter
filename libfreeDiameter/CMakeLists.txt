# Name of the subproject
Project("libfreeDiameter" C)

SET(PROJECT_COPYRIGHT "Copyright (c) 2008-2009, WIDE Project (www.wide.ad.jp) and NICT (www.nict.go.jp)")

# List of source files for the library
SET(LFD_SRC
	libfD.h
	dictionary.c
	dispatch.c
	init.c
	lists.c
	log.c
	messages.c
	mqueues.c
	sessions.c
	)

# Build as a shared library
ADD_LIBRARY(libfreeDiameter SHARED ${LFD_SRC})

# Avoid the liblib name
SET_TARGET_PROPERTIES(libfreeDiameter PROPERTIES OUTPUT_NAME "freeDiameter")

# The library itself needs other libraries 
TARGET_LINK_LIBRARIES(libfreeDiameter ${FD_LIBS})

# Save the list of files for testcases in the daemon's directory
SET(LFD_SRC ${LFD_SRC} PARENT_SCOPE)

