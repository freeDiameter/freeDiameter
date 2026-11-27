PROJECT("DNS discovery routing extension" C)

BISON_FILE(rd_conf.y)
FLEX_FILE(rd_conf.l)
SET_SOURCE_FILES_PROPERTIES(lex.rd_conf.c rd_conf.tab.c PROPERTIES COMPILE_FLAGS "-I ${CMAKE_CURRENT_SOURCE_DIR}")
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

FD_ADD_EXTENSION(rt_discover rt_discover.c rt_discover.h
	lex.rd_conf.c rd_conf.tab.c rd_conf.tab.h)

INSTALL(TARGETS rt_discover
	LIBRARY DESTINATION ${INSTALL_EXTENSIONS_SUFFIX}
	COMPONENT freeDiameter-daemon)
