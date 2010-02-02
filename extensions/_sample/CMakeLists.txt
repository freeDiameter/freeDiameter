# The sample extension
PROJECT("Sample extension" C)

# Overwrite the debug level for the extension code if configured
OPTION(DEBUG_LEVEL__SAMPLE "Overwrite debug level for the extension _sample if defined" OFF)
IF (DEBUG_LEVEL__SAMPLE)
     ADD_DEFINITIONS(-DTRACE_LEVEL=${DEBUG_LEVEL__SAMPLE})
ENDIF (DEBUG_LEVEL__SAMPLE)

# Compile as a module
FD_ADD_EXTENSION(dbg_sample sample.c fini.c)
