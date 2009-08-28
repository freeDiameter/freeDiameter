# This file is the source for generating the Makefile for the project, using cmake tool (cmake.org)

# Name of the project, and language
PROJECT("FreeDiameter" C)

# Some subfolders may have tests
ENABLE_TESTING()

# CMake version
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

# Location of additional CMake modules
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# All source code should be POSIX 200112L compatible, but some other extensions might be used, so:
ADD_DEFINITIONS(-D_GNU_SOURCE)

# Location for the include files
INCLUDE_DIRECTORIES(include)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR}/include)
SUBDIRS(include/freediameter)

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

# Location for the source code
SUBDIRS(libfreediameter)
SUBDIRS(freediameter)

# Do we build the extensions?
OPTION(IGNORE_ALL_EXTENSIONS "Ignore the extensions completly?")
IF(NOT IGNORE_ALL_EXTENSIONS)
	SUBDIRS(extensions)
ENDIF(NOT IGNORE_ALL_EXTENSIONS)
