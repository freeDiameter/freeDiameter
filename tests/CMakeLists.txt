# Test directory
PROJECT("freeDiameter tests" C)

# give the possibility to configure the timeout duration for the tests
OPTION(TEST_TIMEOUT "Timeout for the tests, in seconds (default: 120)?")
IF(TEST_TIMEOUT)
	ADD_DEFINITIONS(-DTEST_TIMEOUT=${TEST_TIMEOUT})
ENDIF(TEST_TIMEOUT)

# default command line for the tests
OPTION(OPT_TEST_ARGUMENTS "Specify the default arguments for the tests" OFF)
IF(OPT_TEST_ARGUMENTS)
	SET(TEST_ARGUMENTS ${OPT_TEST_ARGUMENTS})
ELSE(OPT_TEST_ARGUMENTS)
	SET(TEST_ARGUMENTS "")
ENDIF(OPT_TEST_ARGUMENTS)


#############################
# List the test cases
SET(TEST_LIST
	testsctp
	testostr
	testfifo
	testpeers
	testdict
	testmesg
	testmesg_stress
	testsess
	testdisp
	testcnx
	testloadext
	testserver
)

#############################
# Some parameters for the tests

ADD_DEFINITIONS(-DTEST_DEBUG)
ADD_DEFINITIONS(-DBUILD_DIR="${CMAKE_BINARY_DIR}")

INCLUDE_DIRECTORIES( "../libfdproto" )
INCLUDE_DIRECTORIES( "../libfdcore" )
INCLUDE_DIRECTORIES(${LFDCORE_INCLUDES})


SET(testcnx_ADDITIONAL_LIB  ${CLOCK_GETTIME_LIBS})
SET(testfifo_ADDITIONAL_LIB ${CLOCK_GETTIME_LIBS})
SET(testsess_ADDITIONAL_LIB ${CLOCK_GETTIME_LIBS})
SET(testloadext_ADDITIONAL_LIB ${CMAKE_DL_LIBS})
SET(testmesg_stress_ADDITIONAL_LIB ${CLOCK_GETTIME_LIBS} ${CMAKE_DL_LIBS})

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
   TARGET_LINK_LIBRARIES(${TEST} libfdproto libfdcore ${GNUTLS_LIBRARIES} ${GCRYPT_LIBRARY} ${${TEST}_ADDITIONAL_LIB})
   ADD_TEST(${TEST} ${EXECUTABLE_OUTPUT_PATH}/${TEST} ${TEST_ARGUMENTS})
ENDFOREACH( TEST )

OPTION(BUILD_MULTI_PEER_TESTS "Enable opt-in tests using local Diameter peers" OFF)
IF(BUILD_TWO_PEER_TESTS OR (BUILD_MULTI_PEER_TESTS AND (TWO_PEER_USE_TLS OR TWO_PEER_USE_SCTP)))
	MESSAGE(FATAL_ERROR "Two-peer test options were renamed: unset BUILD_TWO_PEER_TESTS, TWO_PEER_USE_TLS and TWO_PEER_USE_SCTP, and use BUILD_MULTI_PEER_TESTS, MULTI_PEER_USE_TLS and MULTI_PEER_USE_SCTP instead")
ENDIF()

IF(TARGET dbg_interactive OR BUILD_MULTI_PEER_TESTS)
	IF(CMAKE_VERSION VERSION_LESS 3.12)
		FIND_PACKAGE(PythonInterp 3.6 REQUIRED)
		SET(DBG_INTERACTIVE_TEST_PYTHON ${PYTHON_EXECUTABLE})
	ELSE()
		FIND_PACKAGE(Python3 3.6 REQUIRED COMPONENTS Interpreter)
		SET(DBG_INTERACTIVE_TEST_PYTHON ${Python3_EXECUTABLE})
	ENDIF()
ENDIF()

IF(TARGET dbg_interactive)
	ADD_TEST(NAME test_dbg_interactive
		COMMAND ${DBG_INTERACTIVE_TEST_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/test_dbg_interactive.py
			$<TARGET_FILE:freeDiameterd> $<TARGET_FILE:dbg_interactive>)
ENDIF()

IF(BUILD_MULTI_PEER_TESTS)
	OPTION(MULTI_PEER_USE_TLS "Use TLS for the multi-peer scenarios" OFF)
	OPTION(MULTI_PEER_USE_SCTP "Use SCTP instead of TCP for the multi-peer scenarios" OFF)
	IF(MULTI_PEER_USE_SCTP AND DISABLE_SCTP)
		MESSAGE(FATAL_ERROR "MULTI_PEER_USE_SCTP requires SCTP support; disable DISABLE_SCTP")
	ENDIF()
	IF(MULTI_PEER_USE_TLS)
		FIND_PROGRAM(MULTI_PEER_OPENSSL_EXECUTABLE NAMES openssl)
		IF(NOT MULTI_PEER_OPENSSL_EXECUTABLE)
			MESSAGE(FATAL_ERROR "MULTI_PEER_USE_TLS requires the openssl executable")
		ENDIF()
		SET(MULTI_PEER_TLS_VALUE 1)
	ELSE()
		SET(MULTI_PEER_TLS_VALUE 0)
	ENDIF()
	IF(MULTI_PEER_USE_SCTP)
		SET(MULTI_PEER_SCTP_VALUE 1)
	ELSE()
		SET(MULTI_PEER_SCTP_VALUE 0)
	ENDIF()

	FUNCTION(ADD_MULTI_PEER_TEST TEST SCRIPT)
		ADD_TEST(NAME ${TEST}
			COMMAND ${DBG_INTERACTIVE_TEST_PYTHON} -B ${CMAKE_CURRENT_SOURCE_DIR}/${SCRIPT}
				$<TARGET_FILE:freeDiameterd> ${ARGN})
		SET_TESTS_PROPERTIES(${TEST} PROPERTIES LABELS "multi-peer" TIMEOUT 300
			ENVIRONMENT "FD_MULTI_PEER_TLS=${MULTI_PEER_TLS_VALUE};FD_MULTI_PEER_SCTP=${MULTI_PEER_SCTP_VALUE};FD_MULTI_PEER_OPENSSL=${MULTI_PEER_OPENSSL_EXECUTABLE}")
		SET(MULTI_PEER_HAS_SCENARIOS TRUE PARENT_SCOPE)
	ENDFUNCTION()

	IF(TARGET test_app)
		ADD_MULTI_PEER_TEST(test_two_peer_test_app test_two_peer_test_app.py
			$<TARGET_FILE:test_app>)
		ADD_MULTI_PEER_TEST(test_multi_peer_relay test_multi_peer_relay.py
			$<TARGET_FILE:test_app>)
		FOREACH(EXT rt_default rt_deny_by_size rt_ereg)
			IF(TARGET ${EXT})
				ADD_MULTI_PEER_TEST(test_multi_peer_${EXT} test_multi_peer_routing_extensions.py
					$<TARGET_FILE:test_app> $<TARGET_FILE:${EXT}> ${EXT})
			ENDIF()
		ENDFOREACH()
		IF(TARGET rt_busypeers)
			ADD_MULTI_PEER_TEST(test_multi_peer_rt_busypeers test_multi_peer_busy.py
				$<TARGET_FILE:test_app> $<TARGET_FILE:rt_busypeers>)
		ENDIF()
		IF(TARGET rt_redirect AND TARGET rt_default AND TARGET app_redirect AND TARGET dbg_msg_dumps)
			ADD_MULTI_PEER_TEST(test_multi_peer_rt_redirect test_multi_peer_redirect.py
				$<TARGET_FILE:test_app> $<TARGET_FILE:rt_redirect> $<TARGET_FILE:rt_default>
				$<TARGET_FILE:app_redirect> $<TARGET_FILE:dbg_msg_dumps>)
		ENDIF()
		IF(TARGET rt_rewrite AND TARGET dbg_msg_dumps)
			ADD_MULTI_PEER_TEST(test_multi_peer_rt_rewrite test_multi_peer_message_extensions.py
				$<TARGET_FILE:test_app> $<TARGET_FILE:rt_rewrite> $<TARGET_FILE:dbg_msg_dumps> rt_rewrite)
		ENDIF()
		IF(TARGET dbg_rt AND TARGET dbg_loglevel AND NOT CMAKE_BUILD_TYPE MATCHES "MaxPerformance")
			FOREACH(EXT rt_randomize rt_load_balance)
				IF(TARGET ${EXT})
					ADD_MULTI_PEER_TEST(test_multi_peer_${EXT} test_multi_peer_balancing.py
						$<TARGET_FILE:test_app> $<TARGET_FILE:${EXT}> $<TARGET_FILE:dbg_rt> $<TARGET_FILE:dbg_loglevel> ${EXT})
				ENDIF()
			ENDFOREACH()
		ENDIF()
		IF(TARGET dbg_loglevel)
			FOREACH(EXT dbg_dict_dump dbg_dict_dump_json dbg_loglevel dbg_monitor dbg_msg_dumps dbg_msg_timings)
				IF(TARGET ${EXT})
					ADD_MULTI_PEER_TEST(test_multi_peer_${EXT} test_multi_peer_debug_extensions.py
						$<TARGET_FILE:test_app> $<TARGET_FILE:${EXT}> $<TARGET_FILE:dbg_loglevel> ${EXT})
				ENDIF()
			ENDFOREACH()
			IF(TARGET dbg_rt AND NOT CMAKE_BUILD_TYPE MATCHES "MaxPerformance")
				ADD_MULTI_PEER_TEST(test_multi_peer_dbg_rt test_multi_peer_debug_extensions.py
					$<TARGET_FILE:test_app> $<TARGET_FILE:dbg_rt> $<TARGET_FILE:dbg_loglevel> dbg_rt)
			ENDIF()
		ENDIF()
		IF(TARGET rt_ignore_dh AND TARGET dbg_loglevel)
			ADD_MULTI_PEER_TEST(test_multi_peer_rt_ignore_dh test_multi_peer_rt_ignore_dh.py
				$<TARGET_FILE:test_app> $<TARGET_FILE:rt_ignore_dh> $<TARGET_FILE:dbg_loglevel>)
		ENDIF()
		IF(TARGET dbg_interactive)
			ADD_MULTI_PEER_TEST(test_two_peer_dbg_interactive test_two_peer_dbg_interactive.py
				$<TARGET_FILE:test_app> $<TARGET_FILE:dbg_interactive>)
		ENDIF()
	ENDIF()

	IF(NOT MULTI_PEER_HAS_SCENARIOS)
		MESSAGE(FATAL_ERROR "No multi-peer scenarios available; enable BUILD_TEST_APP=ON")
	ENDIF()

	ADD_TEST(NAME test_multi_peer_harness
		COMMAND ${DBG_INTERACTIVE_TEST_PYTHON} -B ${CMAKE_CURRENT_SOURCE_DIR}/test_multi_peer_harness.py)
	SET_TESTS_PROPERTIES(test_multi_peer_harness PROPERTIES LABELS "multi-peer" TIMEOUT 30)
ENDIF()

####
## INSTALL section ##

# we do not install the tests
