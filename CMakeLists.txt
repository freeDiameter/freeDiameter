# This file is the source for generating the Makefile for the project, using cmake tool (cmake.org)

# Name of the project, and language
PROJECT("freeDiameter" C)

# Informations to display in daemon's help
SET(FD_PROJECT_NAME freeDiameter)
SET(FD_PROJECT_BINARY freeDiameterd)
SET(FD_PROJECT_VERSION_MAJOR 1)
SET(FD_PROJECT_VERSION_MINOR 0)
SET(FD_PROJECT_VERSION_REV 0)
SET(FD_PROJECT_COPYRIGHT "Copyright (c) 2008-2009, WIDE Project (www.wide.ad.jp) and NICT (www.nict.go.jp)")

# Some subfolders may have tests
ENABLE_TESTING()

# CMake version
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

# Location of additional CMake modules
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# The default directories
SET(DEFAULT_CONF_PATH ${CMAKE_INSTALL_PREFIX}/etc/freeDiameter CACHE PATH "Default location of freeDiameter configuration files")

SET(INSTALL_DOCUMENTATION_PATH 	${CMAKE_INSTALL_PREFIX}/doc/freeDiameter	CACHE PATH "Directory where the documentation is installed.")
SET(INSTALL_HEADERS_PATH 	${CMAKE_INSTALL_PREFIX}/include/freeDiameter 	CACHE PATH "Directory where the headers are installed.")
SET(INSTALL_EXTENSIONS_PATH 	${CMAKE_INSTALL_PREFIX}/lib/freeDiameter	CACHE PATH "Directory where the extensions are installed / searched.")
SET(INSTALL_DAEMON_PATH 	${CMAKE_INSTALL_PREFIX}/bin 			CACHE PATH "Directory where the daemon binary is installed.")
SET(INSTALL_LIBRARY_PATH 	${CMAKE_INSTALL_PREFIX}/lib 			CACHE PATH "Directory where the libfreeDiameter library is installed.")

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

# Extensions (there is no point in freeDiameter without any extension)
SUBDIRS(extensions)


####
## INSTALL section ##

INSTALL(FILES README LICENSE doc/freediameter.conf.sample
	DESTINATION ${INSTALL_DOCUMENTATION_PATH}
	COMPONENT freeDiameter-daemon)



####
## Packaging section ##

# Default: build a debian package
SET(CPACK_GENERATOR DEB)

# Parameters for the package
SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "freeDiameter: a Diameter implementation (protocol for Authentication, Authorization and Accounting, evolution of RADIUS)")
SET(CPACK_PACKAGE_VENDOR "This is open software")
SET(CPACK_PACKAGE_VERSION_MAJOR "${FD_PROJECT_VERSION_MAJOR}")
SET(CPACK_PACKAGE_VERSION_MINOR "${FD_PROJECT_VERSION_MINOR}")
SET(CPACK_PACKAGE_VERSION_PATCH "${FD_PROJECT_VERSION_REV}")

SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
SET(CPACK_PACKAGE_CONTACT "Sebastien Decugis <sdecugis@nict.go.jp>")
SET(CPACK_PACKAGE_INSTALL_DIRECTORY "CMake ${CMake_VERSION_MAJOR}.${CMake_VERSION_MINOR}")

SET(CPACK_COMPONENTS_ALL 
	freeDiameter-common 
	freeDiameter-daemon 
	freeDiameter-dictionary-RFC4005
	freeDiameter-dictionary-RFC4072
	freeDiameter-radius-gateway
	freeDiameter-accounting-server
	freeDiameter-dev
	freeDiameter-debug-tools
	)
	
# freeDiameter-common
SET( CPACK_COMPONENT_FREEDIAMETER-COMMON_DISPLAY_NAME "freeDiameter library" )
SET( CPACK_COMPONENT_FREEDIAMETER-COMMON_DESCRIPTION "The library needed by all freeDiameter components" )
SET( CPACK_COMPONENT_FREEDIAMETER-COMMON_REQUIRED ON)

# freeDiameter-daemon
SET( CPACK_COMPONENT_FREEDIAMETER-DAEMON_DISPLAY_NAME "freeDiameter daemon binary" )
SET( CPACK_COMPONENT_FREEDIAMETER-DAEMON_DESCRIPTION "The daemon providing Diameter Base Protocol support" )
SET( CPACK_COMPONENT_FREEDIAMETER-DAEMON_DEPENDS freeDiameter-common)

# freeDiameter-dictionary-RFC4005
SET( CPACK_COMPONENT_FREEDIAMETER-DICTIONARY-RFC4005_DISPLAY_NAME "freeDiameter RFC4005 (NASREQ) dictionary" )
SET( CPACK_COMPONENT_FREEDIAMETER-DAEMON_DESCRIPTION "An extension to the daemon's dictionary, providing commands and AVPs for NASREQ application" )
SET( CPACK_COMPONENT_FREEDIAMETER-DAEMON_DEPENDS freeDiameter-daemon)

# freeDiameter-dictionary-RFC4072
SET( CPACK_COMPONENT_FREEDIAMETER-DICTIONARY-RFC4072_DISPLAY_NAME "freeDiameter RFC4072 (EAP) dictionary" )
SET( CPACK_COMPONENT_FREEDIAMETER-DAEMON_DESCRIPTION "An extension to the daemon's dictionary, providing commands and AVPs for Diameter EAP application" )
SET( CPACK_COMPONENT_FREEDIAMETER-DAEMON_DEPENDS freeDiameter-dictionary-RFC4005)

# freeDiameter-radius-gateway
SET( CPACK_COMPONENT_FREEDIAMETER-RADIUS-GATEWAY_DISPLAY_NAME "RADIUS/Diameter gateway extension for freeDiameter" )
SET( CPACK_COMPONENT_FREEDIAMETER-RADIUS-GATEWAY_DESCRIPTION "This extension provides translation of RADIUS client messages to Diameter servers" )
SET( CPACK_COMPONENT_FREEDIAMETER-RADIUS-GATEWAY_DEPENDS freeDiameter-dictionary-RFC4072)

# freeDiameter-accounting-server
SET( CPACK_COMPONENT_FREEDIAMETER-ACCOUNTING-SERVER_DISPLAY_NAME "Accounting server extension for freeDiameter" )
SET( CPACK_COMPONENT_FREEDIAMETER-ACCOUNTING-SERVER_DESCRIPTION "This extension provides a simple configurable Diameter Base Accounting implementation" )
SET( CPACK_COMPONENT_FREEDIAMETER-ACCOUNTING-SERVER_DEPENDS freeDiameter-daemon)

# freeDiameter-dev
SET( CPACK_COMPONENT_FREEDIAMETER-DEV_DISPLAY_NAME "freeDiameter headers" )
SET( CPACK_COMPONENT_FREEDIAMETER-DEV_DESCRIPTION "Install the headers if you plan to develop new freeDiameter extensions" )
SET( CPACK_COMPONENT_FREEDIAMETER-DEV_DEPENDS freeDiameter-common)

# freeDiameter-debug-tools
SET( CPACK_COMPONENT_FREEDIAMETER-DEBUG-TOOLS_DISPLAY_NAME "freeDiameter debug tools" )
SET( CPACK_COMPONENT_FREEDIAMETER-DEBUG-TOOLS_DESCRIPTION "A set of tools meant for debugging freeDiameter" )
SET( CPACK_COMPONENT_FREEDIAMETER-DEBUG-TOOLS_DEPENDS freeDiameter-daemon)

# This is not working well at this time (may 2010), so leave it commented out for the moment.
# INCLUDE(CPack)


