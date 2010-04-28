# The dict_nasreq extension
PROJECT("Simple Accounting server" C)

########################
# Search for libpg (postgresql package)
FIND_PACKAGE(PostgreSQL REQUIRED)
INCLUDE_DIRECTORIES(${POSTGRESQL_INCLUDE_DIR})

########################
# Parser files
BISON_FILE(acct_conf.y)
FLEX_FILE(acct_conf.l)
SET_SOURCE_FILES_PROPERTIES(lex.acct_conf.c acct_conf.tab.c PROPERTIES COMPILE_FLAGS "-I ${CMAKE_CURRENT_SOURCE_DIR}")

# List of source files
SET( APP_ACCT_SRC
	app_acct.h
	app_acct.c
	lex.acct_conf.c
	acct_conf.tab.c
	acct_conf.tab.h
	acct_db.c
	acct_records.c
)

# Compile as a module
FD_ADD_EXTENSION(app_acct ${APP_ACCT_SRC})
TARGET_LINK_LIBRARIES(app_acct ${POSTGRESQL_LIBRARIES})
