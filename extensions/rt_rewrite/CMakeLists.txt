# The rt_rewrite extension
PROJECT("Routing extension that rewrites messages, replacing and removing AVPs" C)

# List of source files
SET(RT_REWRITE_SRC
	rt_rewrite.c
	lex.rt_rewrite_conf.c
	rt_rewrite_conf.tab.c
	rt_rewrite_conf.tab.h
)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

BISON_FILE(rt_rewrite_conf.y)
FLEX_FILE(rt_rewrite_conf.l)
SET_SOURCE_FILES_PROPERTIES(lex.rt_rewrite_conf.c rt_rewrite_conf.tab.c PROPERTIES COMPILE_FLAGS "-I ${CMAKE_CURRENT_SOURCE_DIR}")

# Compile these files as a freeDiameter extension
FD_ADD_EXTENSION(rt_rewrite ${RT_REWRITE_SRC})

####
## INSTALL section ##

# We install with the daemon component because it is a base feature.
INSTALL(TARGETS rt_rewrite
	LIBRARY DESTINATION ${INSTALL_EXTENSIONS_SUFFIX}
	COMPONENT freeDiameter-daemon)
