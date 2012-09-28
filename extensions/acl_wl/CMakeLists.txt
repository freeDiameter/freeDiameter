# The acl_wl extension
PROJECT("Access Control / White List" C)

# Parser files
BISON_FILE(aw_conf.y)
FLEX_FILE(aw_conf.l)
SET_SOURCE_FILES_PROPERTIES(lex.aw_conf.c aw_conf.tab.c PROPERTIES COMPILE_FLAGS "-I ${CMAKE_CURRENT_SOURCE_DIR}")

# List of source files
SET( ACL_WL_SRC
	acl_wl.h
	acl_wl.c
	aw_tree.c
	lex.aw_conf.c
	aw_conf.tab.c
	aw_conf.tab.h
)

# Overwrite the debug level for the extension code if configured
OPTION(DEBUG_LEVEL_ACL_WL "Overwrite debug level for the extension acl_wl if defined (valid values: FULL, ANNOYING, CALL...)" OFF)
IF (DEBUG_LEVEL_ACL_WL)
     ADD_DEFINITIONS(-DTRACE_LEVEL=${DEBUG_LEVEL_ACL_WL})
ENDIF (DEBUG_LEVEL_ACL_WL)
IF (NOT CMAKE_BUILD_TYPE MATCHES "Debug")
  MARK_AS_ADVANCED(DEBUG_LEVEL_ACL_WL)
ENDIF (NOT CMAKE_BUILD_TYPE MATCHES "Debug")

# Compile as a module
FD_ADD_EXTENSION(acl_wl ${ACL_WL_SRC})


####
## INSTALL section ##

# We install with the daemon component because it is a base feature.
INSTALL(TARGETS acl_wl
	LIBRARY DESTINATION ${INSTALL_EXTENSIONS_SUFFIX}
	COMPONENT freeDiameter-daemon)
