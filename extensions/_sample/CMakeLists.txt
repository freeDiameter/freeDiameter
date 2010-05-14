# The sample extension
PROJECT("Sample extension" C)

# Overwrite the debug level for the extension code if configured
OPTION(DEBUG_LEVEL__SAMPLE "Overwrite debug level for the extension _sample if defined (valid values: FULL, ANNOYING, CALL...)" OFF)
IF (DEBUG_LEVEL__SAMPLE)
     ADD_DEFINITIONS(-DTRACE_LEVEL=${DEBUG_LEVEL__SAMPLE})
ENDIF (DEBUG_LEVEL__SAMPLE)
IF (NOT CMAKE_BUILD_TYPE MATCHES "Debug")
  MARK_AS_ADVANCED(DEBUG_LEVEL__SAMPLE)
ENDIF (NOT CMAKE_BUILD_TYPE MATCHES "Debug")

# Compile as a module
FD_ADD_EXTENSION(dbg_sample sample.c fini.c)


####
## INSTALL section ##

INSTALL(TARGETS dbg_sample
	LIBRARY DESTINATION ${INSTALL_EXTENSIONS_PATH}
	COMPONENT freeDiameter-debug-tools)
