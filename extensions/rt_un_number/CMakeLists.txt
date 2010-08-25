# The rt_default extension
PROJECT("Numeric User-Name AVP based load-balancing extension" C)

# Parser files
BISON_FILE(rtun_conf.y)
FLEX_FILE(rtun_conf.l)
SET_SOURCE_FILES_PROPERTIES(lex.rtun_conf.c rtun_conf.tab.c PROPERTIES COMPILE_FLAGS "-I ${CMAKE_CURRENT_SOURCE_DIR}")

# List of source files
SET( RTUN_SRC
	rtun.c
	rtun.h
	lex.rtun_conf.c
	rtun_conf.tab.c
	rtun_conf.tab.h
)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

# Compile these files as a freeDiameter extension
FD_ADD_EXTENSION(rt_un_number ${RTUN_SRC})


####
## INSTALL section ##

# We install with the daemon component because it is a base feature.
INSTALL(TARGETS rt_un_number
	LIBRARY DESTINATION ${INSTALL_EXTENSIONS_SUFFIX}
	COMPONENT freeDiameter-rt-un-number)
