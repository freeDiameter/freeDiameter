#CMake configuration for freeDiameter include directory

Project("freeDiameter includes directory" C)

########################
# Configurable parameters

# Disable SCTP support completely ?
OPTION(DISABLE_SCTP "Disable SCTP support?" OFF)
IF (NOT DISABLE_SCTP)
	OPTION(DEBUG_SCTP "Verbose SCTP (for debug)?" OFF)
	OPTION(SCTP_USE_MAPPED_ADDRESSES "Use v6-mapped v4 addresses in SCTP (workaround some SCTP limitations)?" OFF)
ENDIF (NOT DISABLE_SCTP)

# Find TODO items in the code easily ?
OPTION(ERRORS_ON_TODO "(development) Generate compilation errors on TODO items ?" OFF)

# Create the absolute path for searching extensions
SET(DEFAULT_EXTENSIONS_PATH ${CMAKE_INSTALL_PREFIX}/${INSTALL_EXTENSIONS_SUFFIX})

# IDNA considerations
OPTION(DIAMID_IDNA_IGNORE "Ignore completely invalid characters in Diameter Identities (process blindly)?" OFF)
IF (NOT DIAMID_IDNA_IGNORE)
	OPTION (DIAMID_IDNA_REJECT "Reject internationalized Diameter Identities, do not attempt to convert it (stringprep) ?" OFF)
ENDIF (NOT DIAMID_IDNA_IGNORE)

MARK_AS_ADVANCED(DISABLE_SCTP DEBUG_SCTP SCTP_USE_MAPPED_ADDRESSES ERRORS_ON_TODO DIAMID_IDNA_IGNORE DIAMID_IDNA_REJECT)

########################
### System checks part

INCLUDE (CheckLibraryExists)
INCLUDE (CheckFunctionExists)
INCLUDE (CheckIncludeFiles)
INCLUDE (CheckSymbolExists) 
INCLUDE (CheckCSourceCompiles)
INCLUDE (TestBigEndian)


### System checks -- mandatory support

# We need the getopt_long function
CHECK_FUNCTION_EXISTS (getopt_long HAVE_LONG_OPTIONS)
IF (NOT HAVE_LONG_OPTIONS)
   MESSAGE(SEND_ERROR "The getopt_long function is not found, please add needed library in build system")
ENDIF (NOT HAVE_LONG_OPTIONS)

# getifaddrs ?
CHECK_FUNCTION_EXISTS (getifaddrs HAVE_GETIFADDRS)
IF (NOT HAVE_GETIFADDRS)
   MESSAGE(SEND_ERROR "The getifaddrs function is currently required by freeDiameter.")
ENDIF (NOT HAVE_GETIFADDRS)


### System checks -- for freeDiameter-host.h

# Check byte ordering
TEST_BIG_ENDIAN(HOST_BIG_ENDIAN)

# Check if ntohll is provided on the system
CHECK_SYMBOL_EXISTS(ntohll "" HAVE_NTOHLL)

# malloc.h ?
CHECK_INCLUDE_FILES (malloc.h HAVE_MALLOC_H)

# strndup ? Missing on OS X
CHECK_FUNCTION_EXISTS (strndup HAVE_STRNDUP)


### System checks -- for includes / link

# pthreads
INCLUDE(FindThreads)
SET(CMAKE_THREAD_LIBS_INIT ${CMAKE_THREAD_LIBS_INIT} PARENT_SCOPE)

# clock_gettime
SET(HAVE_CLOCK_GETTIME "")
CHECK_FUNCTION_EXISTS (clock_gettime HAVE_NATIVE_CLOCK_GETTIME)
IF (HAVE_NATIVE_CLOCK_GETTIME)
   SET(CLOCK_GETTIME_LIBS "")
   SET(HAVE_CLOCK_GETTIME 1)
ELSE (HAVE_NATIVE_CLOCK_GETTIME)
   CHECK_LIBRARY_EXISTS (rt clock_gettime "" HAVE_LIBRT)
   IF (HAVE_LIBRT)
     SET(CLOCK_GETTIME_LIBS "-lrt")
     SET(HAVE_CLOCK_GETTIME 1)
   ELSE (HAVE_LIBRT)
     CHECK_LIBRARY_EXISTS (posix4 clock_gettime "" HAVE_LIBPOSIX4)
     IF (HAVE_LIBPOSIX4)
       SET(CLOCK_GETTIME_LIBS "-lposix4")
       SET(HAVE_CLOCK_GETTIME 1)
     ENDIF (HAVE_LIBPOSIX4)
   ENDIF (HAVE_LIBRT)
ENDIF (HAVE_NATIVE_CLOCK_GETTIME)
SET(CLOCK_GETTIME_LIBS ${CLOCK_GETTIME_LIBS} PARENT_SCOPE)

# dlopen and dlclose: CMAKE_DL_LIBS

# We need the sctp_connectx function among others
# We need the IPPROTO_SCTP symbol from sys/socket.h, netinet/in.h or netinet/sctp.h
IF(NOT DISABLE_SCTP)
	CHECK_FUNCTION_EXISTS(sctp_connectx HAVE_NATIVE_SCTP)
	IF(NOT HAVE_NATIVE_SCTP)
		FIND_PACKAGE(SCTP REQUIRED)
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
SET(SCTP_INCLUDE_DIR ${SCTP_INCLUDE_DIR} PARENT_SCOPE)
SET(SCTP_LIBRARIES ${SCTP_LIBRARIES} PARENT_SCOPE)

# IDNA process: we use libidn from GNU (unless the function & header files are included in libc)
IF(NOT DIAMID_IDNA_IGNORE  AND NOT DIAMID_IDNA_REJECT)
	FIND_PACKAGE(IDNA)
   	SET(CHECK_IDNA_SOURCE_CODE "
		#include <idna.h>
		int main() {
		   return idna_to_ascii_8z(NULL, NULL, 0);
		}
		")
	SET(CMAKE_REQUIRED_INCLUDES ${IDNA_INCLUDE_DIR})
	SET(CMAKE_REQUIRED_LIBRARIES ${IDNA_LIBRARIES})
	CHECK_C_SOURCE_COMPILES("${CHECK_IDNA_SOURCE_CODE}" HAS_IDNA_SUPPORT)
	IF(NOT HAS_IDNA_SUPPORT)
		MESSAGE(SEND_ERROR "Unable to find idna.h header or idna_to_ascii_8z function, please install libidn-dev or equivalent, or set DIAMID_IDNA_IGNORE or DIAMID_IDNA_REJECT")
	ENDIF(NOT HAS_IDNA_SUPPORT)
ELSE (NOT DIAMID_IDNA_IGNORE  AND NOT DIAMID_IDNA_REJECT)
	MESSAGE(STATUS "Non-default Internationalized Domain Names (IDN) behavior selected (no stringprep).")
ENDIF(NOT DIAMID_IDNA_IGNORE  AND NOT DIAMID_IDNA_REJECT)
SET(IDNA_INCLUDE_DIR ${IDNA_INCLUDE_DIR} PARENT_SCOPE)
SET(IDNA_LIBRARIES ${IDNA_LIBRARIES} PARENT_SCOPE)


# Require GNU TLS for building the library
FIND_PACKAGE(GnuTLS REQUIRED)
SET(GNUTLS_INCLUDE_DIR ${GNUTLS_INCLUDE_DIR} PARENT_SCOPE)
SET(GNUTLS_LIBRARIES ${GNUTLS_LIBRARIES} PARENT_SCOPE)

find_path(GCRYPT_INCLUDE_DIR NAMES gcrypt.h)
If ( NOT GCRYPT_INCLUDE_DIR )
	MESSAGE(SEND_ERROR "Unable to find gcrypt.h, please install libgcrypt-dev or equivalent")
Endif ( NOT GCRYPT_INCLUDE_DIR )
MARK_AS_ADVANCED(GCRYPT_INCLUDE_DIR)
SET(GCRYPT_INCLUDE_DIR ${GCRYPT_INCLUDE_DIR} PARENT_SCOPE)

# Also we need libgcrypt to... display its version :(
find_library(GCRYPT_LIBRARY 
  NAMES gcrypt
)
If ( NOT GCRYPT_LIBRARY )
	MESSAGE(SEND_ERROR "Unable to find libgcrypt, please install libgcrypt or equivalent")
Endif ( NOT GCRYPT_LIBRARY )
SET(GCRYPT_LIBRARY ${GCRYPT_LIBRARY} PARENT_SCOPE)


# Check if AI_ADDRCONFIG is available on the system
CHECK_SYMBOL_EXISTS(AI_ADDRCONFIG "netdb.h" HAVE_AI_ADDRCONFIG)


# Check if barriers are available (for test_fifo)
SET(CMAKE_REQUIRED_INCLUDES "pthread.h")
SET(CMAKE_REQUIRED_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
CHECK_FUNCTION_EXISTS (pthread_barrier_wait HAVE_PTHREAD_BAR)
SET(HAVE_PTHREAD_BAR ${HAVE_PTHREAD_BAR} PARENT_SCOPE)


##########################

# LFDPROTO_LIBS = libraries required by the libfdproto.
SET(LFDPROTO_LIBS ${CLOCK_GETTIME_LIBS} ${CMAKE_THREAD_LIBS_INIT} ${IDNA_LIBRARIES} PARENT_SCOPE)
# And includes paths
SET(LFDPROTO_INCLUDES ${IDNA_INCLUDE_DIR} PARENT_SCOPE)
# Dependencies: the libraries required by any code linking to libfdproto.
SET(LFDPROTO_LINK_INTERFACES ${CMAKE_THREAD_LIBS_INIT} PARENT_SCOPE)

# LFDCORE_LIBS = libraries required by the libfdcore (in addition to libfdproto and its dependencies)
SET(LFDCORE_LIBS ${CLOCK_GETTIME_LIBS} ${CMAKE_DL_LIBS} ${SCTP_LIBRARIES} ${GCRYPT_LIBRARY} ${GNUTLS_LIBRARIES} PARENT_SCOPE)
# And includes paths
SET(LFDCORE_INCLUDES ${SCTP_INCLUDE_DIR} ${GNUTLS_INCLUDE_DIR} ${GCRYPT_INCLUDE_DIR} PARENT_SCOPE)
# And dependencies
SET(LFDCORE_LINK_INTERFACES "" PARENT_SCOPE) 
		# We don't force other libraries, the programs will link with what it needs
                # (such as libgnutls if it uses GNUTLS_DEBUG() macro
		# or libfdproto if it uses some of its interfaces directly)
		# See freeDiameterd/CMakeLists.txt for an example.

##########################

# Generate the host.h file
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/freeDiameter-host.h.in ${CMAKE_CURRENT_BINARY_DIR}/freeDiameter-host.h)

####
## INSTALL section ##

# The headers from this directory are required to develop new extensions for freeDiameter.
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/freeDiameter-host.h libfdproto.h libfdcore.h extension.h
	DESTINATION ${INSTALL_HEADERS_SUFFIX}
	COMPONENT freeDiameter-dev)

