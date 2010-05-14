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

# Build as a shared library
ADD_LIBRARY(libfreeDiameter SHARED ${LFD_SRC})

# Avoid the liblib name
SET_TARGET_PROPERTIES(libfreeDiameter PROPERTIES OUTPUT_NAME "freeDiameter")

# The library itself needs other libraries 
TARGET_LINK_LIBRARIES(libfreeDiameter ${FD_LIBS})

# Save the list of files for testcases in the daemon's directory
SET(LFD_SRC ${LFD_SRC} PARENT_SCOPE)


####
## INSTALL section ##

INSTALL(TARGETS libfreeDiameter
	LIBRARY DESTINATION ${INSTALL_LIBRARY_PATH}
	COMPONENT freeDiameter-common)

