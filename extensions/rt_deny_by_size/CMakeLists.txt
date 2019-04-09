# The rt_deny_by_size extension
PROJECT("Routing extension to deny messages that are over a particular size" C)

# Parser files
BISON_FILE(rt_deny_by_size_conf.y)
FLEX_FILE(rt_deny_by_size_conf.l)
SET_SOURCE_FILES_PROPERTIES(lex.rt_deny_by_size_conf.c rt_deny_by_size_conf.tab.c PROPERTIES COMPILE_FLAGS "-I ${CMAKE_CURRENT_SOURCE_DIR}")

# List of source files
SET( RT_DENY_BY_SIZE_SRC
	rt_deny_by_size.c
	rt_deny_by_size.h
	lex.rt_deny_by_size_conf.c
	rt_deny_by_size_conf.tab.c
	rt_deny_by_size_conf.tab.h
)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

# Compile these files as a freeDiameter extension
FD_ADD_EXTENSION(rt_deny_by_size ${RT_DENY_BY_SIZE_SRC})

####
## INSTALL section ##

# We install with the daemon component because it is a base feature.
INSTALL(TARGETS rt_deny_by_size
	LIBRARY DESTINATION ${INSTALL_EXTENSIONS_SUFFIX}
	COMPONENT freeDiameter-daemon)
