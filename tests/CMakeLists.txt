# Test directory
PROJECT("freeDiameter tests" C)

# give the possibility to configure the timeout duration for the tests
OPTION(TEST_TIMEOUT "Timeout for the tests, in seconds (default: 5)?")
IF(TEST_TIMEOUT)
	ADD_DEFINITIONS(-DTEST_TIMEOUT=${TEST_TIMEOUT})
ENDIF(TEST_TIMEOUT)


#############################
# List the test cases
SET(TEST_LIST
	testsctp
	testlist
	testdict
	testmesg
	testfifo
	testsess
	testdisp
	testcnx
)

#############################
# Some parameters for the tests

ADD_DEFINITIONS(-DTEST_DEBUG)
ADD_DEFINITIONS(-DTRACE_LEVEL=NONE)

INCLUDE_DIRECTORIES( "../libfdproto" )
INCLUDE_DIRECTORIES( "../libfdcore" )


##############################
# App_acct test

IF(BUILD_APP_ACCT OR ALL_EXTENSIONS)
	OPTION(TEST_APP_ACCT "Test app_acct extension? (Requires a configured database, see testappacct.c for details)" OFF)
	IF(TEST_APP_ACCT)
	
		OPTION(TEST_APP_ACCT_CONNINFO "The connection string to the database")
		IF(TEST_APP_ACCT_CONNINFO)
			ADD_DEFINITIONS(-DTEST_CONNINFO="${TEST_APP_ACCT_CONNINFO}")
		ENDIF(TEST_APP_ACCT_CONNINFO)
	
		SET(TEST_LIST ${TEST_LIST} testappacct)

		# Extension dependencies
		FIND_PACKAGE(PostgreSQL REQUIRED)
		INCLUDE_DIRECTORIES(${POSTGRESQL_INCLUDE_DIR})
		SET(testappacct_ADDITIONAL_LIB ${POSTGRESQL_LIBRARIES})

		# List of source files, copied from the extension CMakeLists.
		BISON_FILE(../extensions/app_acct/acct_conf.y)
		FLEX_FILE(../extensions/app_acct/acct_conf.l)
		#SET_SOURCE_FILES_PROPERTIES(lex.acct_conf.c acct_conf.tab.c PROPERTIES COMPILE_FLAGS "-I ${CMAKE_CURRENT_SOURCE_DIR}")
		
		SET( APP_ACCT_SRC
			app_acct.h
			app_acct.c
			acct_db.c
			acct_records.c
		)
		SET( APP_ACCT_SRC_GEN
			lex.acct_conf.c
			acct_conf.tab.c
			acct_conf.tab.h
		)

		# The extension headers
		INCLUDE_DIRECTORIES( "../extensions/app_acct" )

		SET(testappacct_ADDITIONAL "")

		FOREACH( SRC_FILE ${APP_ACCT_SRC})
		   SET(testappacct_ADDITIONAL ${testappacct_ADDITIONAL} "../extensions/app_acct/${SRC_FILE}")
		ENDFOREACH(SRC_FILE)

		FOREACH( SRC_FILE ${APP_ACCT_SRC_GEN})
		   SET(testappacct_ADDITIONAL ${testappacct_ADDITIONAL} "${CMAKE_CURRENT_BINARY_DIR}/../extensions/app_acct/${SRC_FILE}")
		ENDFOREACH(SRC_FILE)

	ENDIF(TEST_APP_ACCT)
ENDIF(BUILD_APP_ACCT OR ALL_EXTENSIONS)


#############################
# Compile each test
FOREACH( TEST ${TEST_LIST} )
   ADD_EXECUTABLE(${TEST} ${TEST}.c tests.h ${${TEST}_ADDITIONAL})
   TARGET_LINK_LIBRARIES(${TEST} libfdproto libfdcore ${${TEST}_ADDITIONAL_LIB})
   ADD_TEST(${TEST} ${EXECUTABLE_OUTPUT_PATH}/${TEST})
ENDFOREACH( TEST )


####
## INSTALL section ##

# we do not install the tests
