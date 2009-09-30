#CMake configuration for freeDiameter include directory

Project("freeDiameter include directory" C)

########################
# Configurable parameters

# Name of the default configuration file
OPTION(DEFAULT_CONF_FILE "Default path to configuration file?" OFF)

# Disable SCTP support completly ?
OPTION(DISABLE_SCTP "Disable SCTP support?")

# Disable SCTP support completly ?
OPTION(ERRORS_ON_TODO "(development) Generate compilation errors on TODO items ?" OFF)


########################
# System checks 

INCLUDE (CheckLibraryExists)
INCLUDE (CheckFunctionExists)
INCLUDE (CheckIncludeFiles)
INCLUDE (CheckSymbolExists) 
INCLUDE (TestBigEndian)

# We use dlopen and dlclose
SET(FD_LIBS ${FD_LIBS} ${CMAKE_DL_LIBS})

# We need the threads library (pthread)
INCLUDE(FindThreads)
SET(FD_LIBS ${FD_LIBS} ${CMAKE_THREAD_LIBS_INIT})

# We need the clock_gettime function ( -lrt, -lposix4 )
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
SET(FD_LIBS ${FD_LIBS} ${CLOCK_GETTIME_LIBS})

# We need the sctp_getladdrs function ( -lsctp )
# We need the IPPROTO_SCTP symbol from sys/socket.h, netinet/in.h or netinet/sctp.h
IF(NOT DISABLE_SCTP)
	CHECK_FUNCTION_EXISTS(sctp_getladdrs HAVE_NATIVE_SCTP)
	IF(NOT HAVE_NATIVE_SCTP)
		FIND_PACKAGE(SCTP REQUIRED)
		INCLUDE_DIRECTORIES(${SCTP_INCLUDE_DIRS})
		SET(FD_LIBS ${FD_LIBS} ${SCTP_LIBRARIES})
	ENDIF(NOT HAVE_NATIVE_SCTP)
ENDIF(NOT DISABLE_SCTP)

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

# signalent.h ? -- found in strace distrib; just for nice signal names...
CHECK_INCLUDE_FILES (signalent.h HAVE_SIGNALENT_H)

# The default configuration file name
IF (NOT DEFAULT_CONF_FILE)
   SET(DEFAULT_CONF_FILE "freeDiameter.conf")
ENDIF (NOT DEFAULT_CONF_FILE)
##########################

# Generate the host.h file
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/freeDiameter-host.h.in ${CMAKE_CURRENT_BINARY_DIR}/freeDiameter-host.h)

# Save the FD_LIBS for parent scope
SET(FD_LIBS ${FD_LIBS} PARENT_SCOPE)
