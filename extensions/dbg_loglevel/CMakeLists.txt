# The dbg_loglevel extension
PROJECT("Debugging extension that allows changing the log level at runtime" C)

# Parser files
BISON_FILE(dbg_loglevel_conf.y)
FLEX_FILE(dbg_loglevel_conf.l)
SET_SOURCE_FILES_PROPERTIES(lex.dbg_loglevel_conf.c dbg_loglevel_conf.tab.c PROPERTIES COMPILE_FLAGS "-I ${CMAKE_CURRENT_SOURCE_DIR}")

# List of source files
SET(DBG_LOGLEVEL_SRC
	dbg_loglevel.c
	dbg_loglevel.h
	lex.dbg_loglevel_conf.c
	dbg_loglevel_conf.tab.c
	dbg_loglevel_conf.tab.h
)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

# Compile these files as a freeDiameter extension
FD_ADD_EXTENSION(dbg_loglevel ${DBG_LOGLEVEL_SRC})

####
## INSTALL section ##

INSTALL(TARGETS dbg_loglevel
	LIBRARY DESTINATION ${INSTALL_EXTENSIONS_SUFFIX}
	COMPONENT freeDiameter-debug-tools)
