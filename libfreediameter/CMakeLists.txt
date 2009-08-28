# Name of the subproject
Project("libfreediameter" C)

SET(PROJECT_COPYRIGHT "Copyright (c) 2008-2009, WIDE Project (www.wide.ad.jp) and NICT (www.nict.go.jp)")

# List of source files for the library
SET(LFD_SRC
	libfd.h
	init.c
	log.c
	lists.c
	dictionary.c
	messages.c
	mqueues.c
	)

# Build as a shared library
ADD_LIBRARY(libfreediameter SHARED ${LFD_SRC})

# Avoid the liblib name
SET_TARGET_PROPERTIES(libfreediameter PROPERTIES OUTPUT_NAME "freediameter")

# The library itself needs other libraries 
TARGET_LINK_LIBRARIES(libfreediameter ${FD_LIBS})

