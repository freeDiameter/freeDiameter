# This file is the source for generating the Makefile for the project, using cmake tool (cmake.org)

# Name of the project, and language
PROJECT("freeDiameter" C)

# Informations to display in daemon's help
SET(FD_PROJECT_NAME freeDiameter)
SET(FD_PROJECT_BINARY freeDiameterd)
SET(FD_PROJECT_COPYRIGHT "Copyright (c) 2008-2009, WIDE Project (www.wide.ad.jp) and NICT (www.nict.go.jp)")

# Version of the source code
SET(FD_PROJECT_VERSION_MAJOR 1)
SET(FD_PROJECT_VERSION_MINOR 0)
SET(FD_PROJECT_VERSION_REV 2)

# Version of the API with the library
SET(FD_PROJECT_VERSION_API 1)

# The test framework, using CTest and CDash.
INCLUDE(CTest)

# CMake version
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

# Location of additional CMake modules
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# The default directories
SET(DEFAULT_CONF_PATH ${CMAKE_INSTALL_PREFIX}/etc/freeDiameter CACHE PATH "Default location of freeDiameter configuration files")

SET(INSTALL_HEADERS_SUFFIX 		include/freeDiameter 	CACHE PATH "Directory where the headers are installed (relative to CMAKE_INSTALL_PREFIX).")
SET(INSTALL_EXTENSIONS_SUFFIX 		lib/freeDiameter	CACHE PATH "Directory where the extensions are installed / searched (relative to CMAKE_INSTALL_PREFIX).")
SET(INSTALL_DAEMON_SUFFIX 		bin 			CACHE PATH "Directory where the daemon binary is installed (relative to CMAKE_INSTALL_PREFIX).")
SET(INSTALL_LIBRARY_SUFFIX 		lib 			CACHE PATH "Directory where the libfreeDiameter library is installed (relative to CMAKE_INSTALL_PREFIX).")

# All source code should be POSIX 200112L compatible, but some other extensions might be used, so:
ADD_DEFINITIONS(-D_GNU_SOURCE)

# Add a "Profiling" build type
# SET(CMAKE_BUILD_TYPE Profiling)
SET(CMAKE_C_FLAGS_PROFILING "-fprofile-arcs -ftest-coverage -fstack-protector -g -Wall")

# Add a "DebianPackage" build type used when creating the Debian packages
SET(CMAKE_C_FLAGS_DEBIANPACKAGE "${CMAKE_C_FLAGS_RELWITHDEBINFO}")

# Add a "DebugValgrind" build type used for checking execution with Valgrind tool
SET(CMAKE_C_FLAGS_DEBUGVALGRIND "-Wall -g -O0")

# Set the DEBUG flag for Debug and Profiling builds
IF (CMAKE_BUILD_TYPE MATCHES "Debug|Profiling|DebugValgrind")
  SET(DEBUG 1)
ENDIF (CMAKE_BUILD_TYPE MATCHES "Debug|Profiling|DebugValgrind")

# some subfolders use yacc and lex parsers
SET(BISON_GENERATE_DEFINES TRUE)
SET(BISON_PREFIX_OUTPUTS TRUE)
INCLUDE(CMakeUserUseBison)
SET(FLEX_PREFIX_OUTPUTS TRUE)
INCLUDE(CMakeUserUseFlex)
IF( NOT BISON_EXECUTABLE OR NOT FLEX_EXECUTABLE )
  MESSAGE( SEND_ERROR "Bison and Flex are required" )
ENDIF( NOT BISON_EXECUTABLE OR NOT FLEX_EXECUTABLE )
# Check that flex is at least 2.5.20 to support bison-bridge
# how to do the check with cmake???

# Add this to workaround an apparent bug in FreeBSD cmake (already defined in Linux)
SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "-rdynamic")

# Location for the include files
INCLUDE_DIRECTORIES(include)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR}/include)
SUBDIRS(include/freeDiameter)

# Location for the source code
SUBDIRS(libfreeDiameter)
SUBDIRS(freeDiameter)

# Extensions (there is no use of freeDiameter without any extension)
SUBDIRS(extensions)
