# The dict_nasreq extension
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
OPTION(DEBUG_LEVEL_ACL_WL "Overwrite debug level for the extension acl_wl if defined" OFF)
IF (DEBUG_LEVEL_ACL_WL)
     ADD_DEFINITIONS(-DTRACE_LEVEL=${DEBUG_LEVEL_ACL_WL})
ENDIF (DEBUG_LEVEL_ACL_WL)

# Compile as a module
FD_ADD_EXTENSION(acl_wl ${ACL_WL_SRC})
