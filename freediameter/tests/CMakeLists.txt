# Test directory
PROJECT("libfreediameter tests" C)

# give the possibility to configure the timeout duration for the tests
OPTION(TEST_TIMEOUT "Timeout for the tests, in seconds (default: 5)?")
IF(TEST_TIMEOUT)
	ADD_DEFINITIONS(-DTEST_TIMEOUT=${TEST_TIMEOUT})
ENDIF(TEST_TIMEOUT)


#############################
# List the test cases
SET(TEST_LIST
	testdict
	testmesg
	testmq
)

#############################
# Some parameters for the tests

# Add this flag to add some debug information in the tests themselves
ADD_DEFINITIONS(-DTEST_DEBUG)

INCLUDE_DIRECTORIES( ".." )

SET(TEST_COMMON_SRC "")

FOREACH( SRC_FILE ${FD_COMMON_SRC})
   SET(TEST_COMMON_SRC ${TEST_COMMON_SRC} "../${SRC_FILE}")
ENDFOREACH(SRC_FILE)

# FOREACH( SRC_FILE ${FD_COMMON_GEN_SRC})
#    SET(TEST_COMMON_SRC ${TEST_COMMON_SRC} "${CMAKE_CURRENT_BINARY_DIR}/../${SRC_FILE}")
# ENDFOREACH(SRC_FILE)

# Create an archive with the daemon common files
ADD_LIBRARY(fdcore STATIC ${TEST_COMMON_SRC})


#############################
# Compile each test
FOREACH( TEST ${TEST_LIST} )
   ADD_EXECUTABLE(${TEST} ${TEST}.c tests.h)
   TARGET_LINK_LIBRARIES(${TEST} libfreediameter fdcore ${FD_LIBS})
   ADD_TEST(${TEST} ${EXECUTABLE_OUTPUT_PATH}/${TEST})
ENDFOREACH( TEST )
