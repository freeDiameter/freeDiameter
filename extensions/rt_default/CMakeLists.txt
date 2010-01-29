# The rt_default extension
PROJECT("Configurable routing extension" C)

# Parser files
BISON_FILE(rtd_conf.y)
FLEX_FILE(rtd_conf.l)
SET_SOURCE_FILES_PROPERTIES(lex.rtd_conf.c rtd_conf.tab.c PROPERTIES COMPILE_FLAGS "-I ${CMAKE_CURRENT_SOURCE_DIR}")

# List of source files
SET( RT_DEFAULT_SRC
	rt_default.c
	rt_default.h
	lex.rtd_conf.c
	rtd_conf.tab.c
	rtd_conf.tab.h
	rtd_rules.c
)

# Compile these files as a freeDiameter extension
FD_ADD_EXTENSION(rt_default ${RT_DEFAULT_SRC})
