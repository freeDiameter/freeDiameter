# Test directory
PROJECT("libfreeDiameter tests" C)

# give the possibility to configure the timeout duration for the tests
OPTION(TEST_TIMEOUT "Timeout for the tests, in seconds (default: 5)?")
IF(TEST_TIMEOUT)
	ADD_DEFINITIONS(-DTEST_TIMEOUT=${TEST_TIMEOUT})
ENDIF(TEST_TIMEOUT)


#############################
# List the test cases
SET(TEST_LIST
	testlist
	testdict
	testmesg
	testqueues
	testsess
	testdisp
)

#############################
# Some parameters for the tests

ADD_DEFINITIONS(-DTEST_DEBUG)
ADD_DEFINITIONS(-DTRACE_LEVEL=NONE)

INCLUDE_DIRECTORIES( ".." )
INCLUDE_DIRECTORIES( "../../libfreeDiameter" )

BISON_FILE(../fdd.y)
FLEX_FILE(../fdd.l)

SET(TEST_COMMON_SRC "")

FOREACH( SRC_FILE ${FD_COMMON_SRC})
   SET(TEST_COMMON_SRC ${TEST_COMMON_SRC} "../${SRC_FILE}")
ENDFOREACH(SRC_FILE)

FOREACH( SRC_FILE ${FD_COMMON_GEN_SRC})
   SET(TEST_COMMON_SRC ${TEST_COMMON_SRC} "${CMAKE_CURRENT_BINARY_DIR}/../${SRC_FILE}")
ENDFOREACH(SRC_FILE)

FOREACH( SRC_FILE ${LFD_SRC})
   SET(TEST_COMMON_SRC ${TEST_COMMON_SRC} "../../libfreeDiameter/${SRC_FILE}")
ENDFOREACH(SRC_FILE)

# Create an archive with the daemon common files (all but main)
ADD_LIBRARY(fDcore STATIC ${TEST_COMMON_SRC})


#############################
# Compile each test
FOREACH( TEST ${TEST_LIST} )
   ADD_EXECUTABLE(${TEST} ${TEST}.c tests.h)
   TARGET_LINK_LIBRARIES(${TEST} fDcore ${FD_LIBS})
   ADD_TEST(${TEST} ${EXECUTABLE_OUTPUT_PATH}/${TEST})
ENDFOREACH( TEST )
