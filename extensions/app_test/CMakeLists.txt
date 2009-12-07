# The dict_nasreq extension
PROJECT("Test Diameter Application" C)

# Parser files
BISON_FILE(atst_conf.y)
FLEX_FILE(atst_conf.l)
SET_SOURCE_FILES_PROPERTIES(lex.atst_conf.c atst_conf.tab.c PROPERTIES COMPILE_FLAGS "-I ${CMAKE_CURRENT_SOURCE_DIR}")

# List of source files
SET( APP_TEST_SRC
	app_test.c
	app_test.h
	lex.atst_conf.c
	atst_conf.tab.c
	atst_conf.tab.h
	atst_sig.c
	atst_dict.c
	atst_serv.c
	atst_cli.c
)

# Compile as a module
FD_ADD_EXTENSION(app_test ${APP_TEST_SRC})
