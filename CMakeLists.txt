# This file is the source for generating the Makefile for the project, using cmake tool (cmake.org)

# Name of the project, and language
PROJECT("freeDiameter" C)

# Informations to display in daemon's help
SET(FD_PROJECT_NAME freeDiameter)
SET(FD_PROJECT_BINARY freeDiameterd)
SET(FD_PROJECT_VERSION_MAJOR 0)
SET(FD_PROJECT_VERSION_MINOR 1)
SET(FD_PROJECT_VERSION_REV 0)
SET(FD_PROJECT_COPYRIGHT "Copyright (c) 2008-2009, WIDE Project (www.wide.ad.jp) and NICT (www.nict.go.jp)")

# Some subfolders may have tests
ENABLE_TESTING()

# CMake version
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

# Location of additional CMake modules
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# All source code should be POSIX 200112L compatible, but some other extensions might be used, so:
ADD_DEFINITIONS(-D_GNU_SOURCE)
IF (CMAKE_BUILD_TYPE MATCHES "Debug")
  SET(DEBUG 1)
ENDIF (CMAKE_BUILD_TYPE MATCHES "Debug")

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

# Location for the include files
INCLUDE_DIRECTORIES(include)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR}/include)
SUBDIRS(include/freeDiameter)

# Location for the source code
SUBDIRS(libfreeDiameter)
SUBDIRS(freeDiameter)

# Do we build the extensions?
OPTION(IGNORE_ALL_EXTENSIONS "Ignore the extensions completly?")
IF(NOT IGNORE_ALL_EXTENSIONS)
	SUBDIRS(extensions)
ENDIF(NOT IGNORE_ALL_EXTENSIONS)
