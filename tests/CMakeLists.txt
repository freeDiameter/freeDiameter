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
   IF("${TEST}" STREQUAL "testcnx" AND ENABLE_DTLS_SCTP)
      ADD_TEST(NAME testcnx COMMAND $<TARGET_FILE:testcnx> ${TEST_ARGUMENTS} -p 2)
      ADD_TEST(NAME testcnx_dtls COMMAND $<TARGET_FILE:testcnx> ${TEST_ARGUMENTS} -p 1)
      SET_TESTS_PROPERTIES(testcnx testcnx_dtls PROPERTIES RESOURCE_LOCK testcnx_ports)
      SET_TESTS_PROPERTIES(testcnx_dtls PROPERTIES SKIP_RETURN_CODE 77)
   ELSE()
      ADD_TEST(${TEST} ${EXECUTABLE_OUTPUT_PATH}/${TEST} ${TEST_ARGUMENTS})
   ENDIF()
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
	OPTION(MULTI_PEER_USE_TLS "Use TLS/DTLS for the multi-peer scenarios" OFF)
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
	IF(ENABLE_DTLS_SCTP)
		SET(MULTI_PEER_DTLS_VALUE 1)
	ELSE()
		SET(MULTI_PEER_DTLS_VALUE 0)
	ENDIF()

	FUNCTION(ADD_MULTI_PEER_TEST TEST SCRIPT)
		ADD_TEST(NAME ${TEST}
			COMMAND ${DBG_INTERACTIVE_TEST_PYTHON} -B ${CMAKE_CURRENT_SOURCE_DIR}/${SCRIPT}
				$<TARGET_FILE:freeDiameterd> ${ARGN})
		SET_TESTS_PROPERTIES(${TEST} PROPERTIES LABELS "multi-peer" TIMEOUT 300 SKIP_RETURN_CODE 77
			ENVIRONMENT "FD_MULTI_PEER_TLS=${MULTI_PEER_TLS_VALUE};FD_MULTI_PEER_SCTP=${MULTI_PEER_SCTP_VALUE};FD_MULTI_PEER_DTLS=${MULTI_PEER_DTLS_VALUE};FD_MULTI_PEER_OPENSSL=${MULTI_PEER_OPENSSL_EXECUTABLE}")
		SET(MULTI_PEER_HAS_SCENARIOS TRUE PARENT_SCOPE)
	ENDFUNCTION()

	ADD_TEST(NAME test_sctp_security_config
		COMMAND ${DBG_INTERACTIVE_TEST_PYTHON} -B ${CMAKE_CURRENT_SOURCE_DIR}/test_sctp_security_config.py
			$<TARGET_FILE:freeDiameterd> ${CMAKE_COMMAND} ${CMAKE_SOURCE_DIR}
			${CMAKE_BINARY_DIR} ${MULTI_PEER_DTLS_VALUE})
	SET_TESTS_PROPERTIES(test_sctp_security_config PROPERTIES LABELS "multi-peer" TIMEOUT 90)

	IF(TARGET test_netemul)
		ADD_EXECUTABLE(test_netemul_process test_netemul_process.c)
		TARGET_LINK_LIBRARIES(test_netemul_process
			$<TARGET_PROPERTY:test_netemul,LINK_LIBRARIES> ${GNUTLS_LIBRARIES} ${GCRYPT_LIBRARY} ${CLOCK_GETTIME_LIBS})
		ADD_TEST(NAME test_netemul_process COMMAND test_netemul_process)
		SET_TESTS_PROPERTIES(test_netemul_process PROPERTIES LABELS "multi-peer")
	ENDIF()

	IF(TARGET dbg_dict_dump_json)
		FUNCTION(ADD_DICTIONARY_EXTENSION_TEST EXT EXPECTED CODE VENDOR)
			FOREACH(DEP ${ARGN} ${EXT})
				IF(NOT TARGET ${DEP})
					RETURN()
				ENDIF()
				LIST(APPEND DICTIONARIES $<TARGET_FILE:${DEP}>)
			ENDFOREACH()
			ADD_MULTI_PEER_TEST(test_dictionary_${EXT} test_dictionary_extension.py
				$<TARGET_FILE:dbg_dict_dump_json> ${EXPECTED}:${CODE}:${VENDOR} ${DICTIONARIES})
			SET(MULTI_PEER_HAS_SCENARIOS TRUE PARENT_SCOPE)
		ENDFUNCTION()

		ADD_DICTIONARY_EXTENSION_TEST(dict_nasreq NAS-Port 5 0)
		ADD_DICTIONARY_EXTENSION_TEST(dict_eap EAP-Payload 462 0 dict_nasreq)
		ADD_DICTIONARY_EXTENSION_TEST(dict_dcca CC-Input-Octets 412 0 dict_nasreq)
		ADD_DICTIONARY_EXTENSION_TEST(dict_dcca_3gpp 3GPP-IMSI 1 10415 dict_nasreq dict_dcca)
		ADD_DICTIONARY_EXTENSION_TEST(dict_dcca_starent SN-Volume-Quota-Threshold 501 8164 dict_nasreq dict_dcca dict_dcca_3gpp)
		ADD_DICTIONARY_EXTENSION_TEST(dict_sip Digest-Response 103 0)
		ADD_DICTIONARY_EXTENSION_TEST(dict_rfc5777 QoS-Profile-Id 573 0)
		ADD_DICTIONARY_EXTENSION_TEST(dict_mip6a Service-Selection 493 0 dict_rfc5777)
		ADD_DICTIONARY_EXTENSION_TEST(dict_mip6i MIP-Mobile-Node-Address 333 0 dict_rfc5777)
		ADD_DICTIONARY_EXTENSION_TEST(dict_nas_mipv6 MIP6-Home-Link-Prefix 125 0)
		ADD_DICTIONARY_EXTENSION_TEST(dict_rfc7944 DRMP 301 0)

		SET(COMBINED_DICTIONARIES dict_nasreq dict_eap dict_dcca dict_dcca_3gpp
			dict_dcca_starent dict_sip dict_rfc5777 dict_mip6a dict_mip6i
			dict_nas_mipv6 dict_rfc7944)
		SET(COMBINED_DICTIONARY_TARGETS "")
		FOREACH(EXT ${COMBINED_DICTIONARIES})
			IF(TARGET ${EXT})
				LIST(APPEND COMBINED_DICTIONARY_TARGETS $<TARGET_FILE:${EXT}>)
			ENDIF()
		ENDFOREACH()
		LIST(LENGTH COMBINED_DICTIONARIES COMBINED_DICTIONARY_COUNT)
		LIST(LENGTH COMBINED_DICTIONARY_TARGETS COMBINED_TARGET_COUNT)
		IF(COMBINED_DICTIONARY_COUNT EQUAL COMBINED_TARGET_COUNT)
			SET(COMBINED_AVPS "NAS-Port:5:0,EAP-Payload:462:0,CC-Input-Octets:412:0,3GPP-IMSI:1:10415,SN-Volume-Quota-Threshold:501:8164,Digest-Response:103:0,QoS-Profile-Id:573:0,Service-Selection:493:0,MIP-Mobile-Node-Address:333:0,MIP6-Home-Link-Prefix:125:0,DRMP:301:0")
			ADD_MULTI_PEER_TEST(test_dictionary_combined test_dictionary_extension.py
				$<TARGET_FILE:dbg_dict_dump_json> ${COMBINED_AVPS} ${COMBINED_DICTIONARY_TARGETS})
		ENDIF()
	ENDIF()

	IF(TARGET test_app)
		ADD_MULTI_PEER_TEST(test_two_peer_test_app test_two_peer_test_app.py
			$<TARGET_FILE:test_app>)
		IF(MULTI_PEER_USE_TLS AND MULTI_PEER_USE_SCTP)
			ADD_MULTI_PEER_TEST(test_two_peer_legacy_sctp test_two_peer_test_app.py
				$<TARGET_FILE:test_app> legacy)
			ADD_MULTI_PEER_TEST(test_two_peer_no_tls_with_legacy_selection test_two_peer_test_app.py
				$<TARGET_FILE:test_app> no-tls-legacy)
			ADD_MULTI_PEER_TEST(test_two_peer_old_method_with_legacy_selection test_two_peer_test_app.py
				$<TARGET_FILE:test_app> old-method-legacy)
		ENDIF()
		IF(MULTI_PEER_USE_TLS AND NOT MULTI_PEER_USE_SCTP)
			ADD_MULTI_PEER_TEST(test_two_peer_tcp_with_legacy_selection test_two_peer_test_app.py
				$<TARGET_FILE:test_app> tcp-legacy)
			IF(ENABLE_DTLS_SCTP)
				ADD_MULTI_PEER_TEST(test_two_peer_tcp_with_dtls_selection test_two_peer_test_app.py
					$<TARGET_FILE:test_app> tcp-dtls)
			ENDIF()
		ENDIF()
		IF(ENABLE_DTLS_SCTP AND MULTI_PEER_USE_TLS AND MULTI_PEER_USE_SCTP)
			ADD_LIBRARY(test_sctp_ack_probe SHARED test_sctp_ack_probe.c)
			TARGET_LINK_LIBRARIES(test_sctp_ack_probe ${CMAKE_DL_LIBS})
			ADD_MULTI_PEER_TEST(test_two_peer_explicit_dtls test_two_peer_test_app.py
				$<TARGET_FILE:test_app> explicit-dtls)
			ADD_MULTI_PEER_TEST(test_multi_peer_mixed_sctp test_multi_peer_mixed_sctp.py
				$<TARGET_FILE:test_app>)
			ADD_MULTI_PEER_TEST(test_sctp_no_auth_tls test_sctp_shared_port.py
				$<TARGET_FILE:test_app> no-auth)
			ADD_MULTI_PEER_TEST(test_sctp_auth_tls test_sctp_shared_port.py
				$<TARGET_FILE:test_app> auth)
			ADD_MULTI_PEER_TEST(test_sctp_fragmented_tls test_sctp_shared_port.py
				$<TARGET_FILE:test_app> fragmented)
			ADD_MULTI_PEER_TEST(test_sctp_no_auth_dtls_rejected test_sctp_shared_port.py
				$<TARGET_FILE:test_app> invalid-dtls)
			ADD_MULTI_PEER_TEST(test_sctp_dtls_without_data_auth test_sctp_shared_port.py
				$<TARGET_FILE:test_app> dtls-no-data-auth)
			ADD_MULTI_PEER_TEST(test_sctp_dtls_wrong_ppid test_sctp_shared_port.py
				$<TARGET_FILE:test_app> wrong-ppid)
			ADD_MULTI_PEER_TEST(test_sctp_idle_handshake test_sctp_shared_port.py
				$<TARGET_FILE:test_app> idle)
			ADD_MULTI_PEER_TEST(test_sctp_outgoing_fallback test_sctp_outgoing_fallback.py
				$<TARGET_FILE:test_app> fallback)
			ADD_MULTI_PEER_TEST(test_sctp_outgoing_strict_dtls test_sctp_outgoing_fallback.py
				$<TARGET_FILE:test_app> strict)
			ADD_MULTI_PEER_TEST(test_sctp_outgoing_strict_local_auth test_sctp_outgoing_fallback.py
				$<TARGET_FILE:test_app> strict-local)
			ADD_MULTI_PEER_TEST(test_sctp_outgoing_no_data_auth test_sctp_outgoing_fallback.py
				$<TARGET_FILE:test_app> no-data-auth)
			ADD_MULTI_PEER_TEST(test_sctp_outgoing_handshake_failure test_sctp_outgoing_fallback.py
				$<TARGET_FILE:test_app> handshake-failure)
			ADD_MULTI_PEER_TEST(test_sctp_openssl_client test_sctp_openssl.py
				$<TARGET_FILE:test_app> client)
			ADD_MULTI_PEER_TEST(test_sctp_openssl_server test_sctp_openssl.py
				$<TARGET_FILE:test_app> server)
			ADD_MULTI_PEER_TEST(test_sctp_netem test_sctp_netem.py
				$<TARGET_FILE:test_app>)
			IF(TARGET dbg_msg_dumps)
				FOREACH(MODE client simultaneous)
					ADD_MULTI_PEER_TEST(test_sctp_shutdown_pending_${MODE} test_sctp_shutdown_pending.py
						$<TARGET_FILE:test_app> $<TARGET_FILE:test_sctp_ack_probe> $<TARGET_FILE:dbg_msg_dumps> ${MODE})
				ENDFOREACH()
			ENDIF()
			FOREACH(MODE handshake-client handshake-server handshake-timeout shutdown shutdown-timeout)
				STRING(REPLACE "-" "_" TEST_MODE ${MODE})
				ADD_MULTI_PEER_TEST(test_sctp_ack_${TEST_MODE} test_sctp_ack_barrier.py
					$<TARGET_FILE:test_app> $<TARGET_FILE:test_sctp_ack_probe> $<TARGET_FILE:testcnx> ${MODE})
			ENDFOREACH()
		ENDIF()
		ADD_MULTI_PEER_TEST(test_multi_peer_relay test_multi_peer_relay.py
			$<TARGET_FILE:test_app>)
		IF(TARGET test_netemul)
			ADD_MULTI_PEER_TEST(test_multi_peer_netemul test_multi_peer_netemul.py
				$<TARGET_FILE:test_app> $<TARGET_FILE:test_netemul>)
		ENDIF()
		IF(TARGET acl_wl)
			ADD_MULTI_PEER_TEST(test_multi_peer_acl_wl test_multi_peer_acl_wl.py
				$<TARGET_FILE:test_app> $<TARGET_FILE:acl_wl>)
		ENDIF()
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
		MESSAGE(FATAL_ERROR "No multi-peer scenarios available; enable BUILD_TEST_APP or BUILD_DBG_DICT_DUMP_JSON and dictionary extensions")
	ENDIF()

	ADD_TEST(NAME test_multi_peer_harness
		COMMAND ${DBG_INTERACTIVE_TEST_PYTHON} -B ${CMAKE_CURRENT_SOURCE_DIR}/test_multi_peer_harness.py)
	SET_TESTS_PROPERTIES(test_multi_peer_harness PROPERTIES LABELS "multi-peer" TIMEOUT 30)
ENDIF()

####
## INSTALL section ##

# we do not install the tests
