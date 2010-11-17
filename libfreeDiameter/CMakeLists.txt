# Name of the subproject
Project("libfreeDiameter" C)

# List of source files for the library
SET(LFD_SRC
	libfD.h
	dictionary.c
	dispatch.c
	fifo.c
	init.c
	lists.c
	log.c
	messages.c
	rt_data.c
	sessions.c
	signal.c
	)

# Save the list of files for testcases in the daemon's directory
SET(LFD_SRC ${LFD_SRC} PARENT_SCOPE)

# Build as a shared library
ADD_LIBRARY(libfreeDiameter SHARED ${LFD_SRC})

# Avoid the liblib name, and set the version
SET_TARGET_PROPERTIES(libfreeDiameter PROPERTIES 
	OUTPUT_NAME "freeDiameter"
	SOVERSION ${FD_PROJECT_VERSION_API}
	VERSION ${FD_PROJECT_VERSION_MAJOR}.${FD_PROJECT_VERSION_MINOR}.${FD_PROJECT_VERSION_REV})

# The library itself needs other libraries 
TARGET_LINK_LIBRARIES(libfreeDiameter ${LFD_LIBS})


####
## INSTALL section ##

INSTALL(TARGETS libfreeDiameter
	LIBRARY DESTINATION ${INSTALL_LIBRARY_SUFFIX}
	COMPONENT freeDiameter-common)

