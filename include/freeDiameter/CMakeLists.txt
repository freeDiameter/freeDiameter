#CMake configuration for freeDiameter include directory

Project("freeDiameter include directory" C)

########################
# Configurable parameters

# Disable SCTP support completly ?
OPTION(DISABLE_SCTP "Disable SCTP support?" OFF)
IF (NOT DISABLE_SCTP)
	OPTION(DEBUG_SCTP "Verbose SCTP (for debug)?" OFF)
	OPTION(SCTP_USE_MAPPED_ADDRESSES "Use v6-mapped v4 addresses in SCTP (workaround some SCTP limitations)?" OFF)
ENDIF (NOT DISABLE_SCTP)

# Find TODO items in the code easily ?
OPTION(ERRORS_ON_TODO "(development) Generate compilation errors on TODO items ?" OFF)

MARK_AS_ADVANCED(DISABLE_SCTP DEBUG_SCTP SCTP_USE_MAPPED_ADDRESSES ERRORS_ON_TODO)

########################
# System checks 

INCLUDE (CheckLibraryExists)
INCLUDE (CheckFunctionExists)
INCLUDE (CheckIncludeFiles)
INCLUDE (CheckSymbolExists) 
INCLUDE (CheckCSourceCompiles)
INCLUDE (TestBigEndian)

# Check byte ordering
TEST_BIG_ENDIAN(HOST_BIG_ENDIAN)

# We need the getopt_long function
CHECK_FUNCTION_EXISTS (getopt_long HAVE_LONG_OPTIONS)
IF (NOT HAVE_LONG_OPTIONS)
   MESSAGE(SEND_ERROR "The getopt_long function is not found, please add needed library in build system")
ENDIF (NOT HAVE_LONG_OPTIONS)

# Check if ntohll is provided on the system
CHECK_SYMBOL_EXISTS(ntohll "" HAVE_NTOHLL)

# malloc.h ?
CHECK_INCLUDE_FILES (malloc.h HAVE_MALLOC_H)

# getifaddrs ?
CHECK_FUNCTION_EXISTS (getifaddrs HAVE_GETIFADDRS)
IF (NOT HAVE_GETIFADDRS)
   MESSAGE(SEND_ERROR "The getifaddrs function is currently required by freeDiameter.")
ENDIF (NOT HAVE_GETIFADDRS)

# pthreads
INCLUDE(FindThreads)

# clock_gettime
CHECK_FUNCTION_EXISTS (clock_gettime HAVE_CLOCK_GETTIME)
IF (HAVE_CLOCK_GETTIME)
   SET(CLOCK_GETTIME_LIBS "")
ELSE (HAVE_CLOCK_GETTIME)
   CHECK_LIBRARY_EXISTS (rt clock_gettime "" HAVE_LIBRT)
   IF (HAVE_LIBRT)
     SET(CLOCK_GETTIME_LIBS "-lrt")
   ELSE (HAVE_LIBRT)
     CHECK_LIBRARY_EXISTS (posix4 clock_gettime "" HAVE_LIBPOSIX4)
     IF (HAVE_LIBPOSIX4)
       SET(CLOCK_GETTIME_LIBS "-lposix4")
     ENDIF (HAVE_LIBPOSIX4)
   ENDIF (HAVE_LIBRT)
ENDIF (HAVE_CLOCK_GETTIME)

# LFD_LIBS = libraries required by the libfreeDiameter.
SET(LFD_LIBS ${CLOCK_GETTIME_LIBS} ${CMAKE_THREAD_LIBS_INIT})


# dlopen and dlclose: CMAKE_DL_LIBS

# We need the sctp_connectx function among others
# We need the IPPROTO_SCTP symbol from sys/socket.h, netinet/in.h or netinet/sctp.h
IF(NOT DISABLE_SCTP)
	CHECK_FUNCTION_EXISTS(sctp_connectx HAVE_NATIVE_SCTP)
	IF(NOT HAVE_NATIVE_SCTP)
		FIND_PACKAGE(SCTP REQUIRED)
		INCLUDE_DIRECTORIES(${SCTP_INCLUDE_DIR})
	ENDIF(NOT HAVE_NATIVE_SCTP)
	# Now check the number of args of this function, since it changed between Ubuntu 9.04 and 9.10
   	SET(CHECK_SCTP_CONNECTX_4_ARGS_SOURCE_CODE "
		#include <unistd.h>
		#include <netinet/sctp.h>
		int main() {
		   return sctp_connectx(0, NULL, 0, NULL);
		}
		")
	SET(CMAKE_REQUIRED_INCLUDES ${SCTP_INCLUDE_DIR})
	SET(CMAKE_REQUIRED_LIBRARIES ${SCTP_LIBRARIES})
	CHECK_C_SOURCE_COMPILES("${CHECK_SCTP_CONNECTX_4_ARGS_SOURCE_CODE}" SCTP_CONNECTX_4_ARGS)
ELSE (NOT DISABLE_SCTP)
	MESSAGE(STATUS "Disabled SCTP support.")
ENDIF(NOT DISABLE_SCTP)


# FD_LIBS = libraries required by the daemon
SET(FD_LIBS ${CMAKE_DL_LIBS} ${SCTP_LIBRARIES})

##########################
# Create the absolute path for searching extensions
SET(DEFAULT_EXTENSIONS_PATH ${CMAKE_INSTALL_PREFIX}/${INSTALL_EXTENSIONS_SUFFIX})

##########################

# Generate the host.h file
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/freeDiameter-host.h.in ${CMAKE_CURRENT_BINARY_DIR}/freeDiameter-host.h)

# Save some variables for the other directories
SET(FD_LIBS  ${FD_LIBS}  PARENT_SCOPE)
SET(LFD_LIBS ${LFD_LIBS} PARENT_SCOPE)

####
## INSTALL section ##

# The headers from this directory are required to develop new extensions for freeDiameter.
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/freeDiameter-host.h libfreeDiameter.h freeDiameter.h extension.h
	DESTINATION ${INSTALL_HEADERS_SUFFIX}
	COMPONENT freeDiameter-dev)

