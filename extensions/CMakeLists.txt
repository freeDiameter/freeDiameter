PROJECT("freeDiameter extensions" C)

# We want all resulting objects in the same folder
SET(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})

# Use the macro FD_ADD_EXTENSION(name files...) to create an extension
# It is equivalent to add_library with the appropriate parameters
# and naming conventions (.fdx : FreeDiameter eXtension)
MACRO(FD_ADD_EXTENSION EXTNAME)
  ADD_LIBRARY(${EXTNAME} MODULE ${ARGN})
  SET_TARGET_PROPERTIES(${EXTNAME} PROPERTIES PREFIX "" )
  SET_TARGET_PROPERTIES(${EXTNAME} PROPERTIES SUFFIX ".fdx" )
ENDMACRO(FD_ADD_EXTENSION)

###########################
# Extensions section

####
# Diameter applications dictionary
OPTION(BUILD_DICT_NASREQ "Build NASREQ (RFC4005) Dictionary definitions?" ON)
	IF (BUILD_DICT_NASREQ)
	   SUBDIRS(dict_nasreq)
	ENDIF (BUILD_DICT_NASREQ)

OPTION(BUILD_DICT_EAP "Build Diameter EAP (RFC4072) Dictionary definitions?" ON)
	IF (BUILD_DICT_EAP)
	   SUBDIRS(dict_eap)
	ENDIF (BUILD_DICT_EAP)



####
# Radius / Diameter gateway extension(s)
# OPTION(BUILD_RADIUS_GW "Build radius_gw? (one-way RADIUS/Diameter gateway - RADIUS NAS <-> Diameter server)" ON)
# 	IF (BUILD_RADIUS_GW)
# 	   SUBDIRS(radius_gw)
# 	ENDIF (BUILD_RADIUS_GW)


####
# Routing extensions

# OPTION(BUILD_RT_DEFAULT "Build rt_default? (Routing extension using Destination-Host, Destination-Realm, and static configuration)" ON)
# 	IF (BUILD_RT_DEFAULT)
# 	   SUBDIRS(rt_default)
# 	ENDIF (BUILD_RT_DEFAULT)


####
# Peers security extensions

# OPTION(BUILD_PS_DEFAULT "Build ps_default? (white-list of peers)" ON)
# 	IF (BUILD_PS_DEFAULT)
# # -- enable once API is updated:	   SUBDIRS(ps_default)
# 	ENDIF (BUILD_PS_DEFAULT)


####
# Debug / development extensions

OPTION(BUILD_SAMPLE "Build sample.fdx? (Simple extension to demonstrate extension mechanism, for developpers only)" OFF)
	IF (BUILD_SAMPLE)
	   SUBDIRS(_sample)
	ENDIF (BUILD_SAMPLE)

OPTION(BUILD_MONITOR "Build monitor.fdx? (display periodical debug information on the console)" OFF)
	IF (BUILD_MONITOR)
	   SUBDIRS(dbg_monitor)
	ENDIF (BUILD_MONITOR)

# OPTION(BUILD_RT_ANY "Build rt_any? (Routing extension randomly sending message to any peer available, for testing purpose only)" OFF)
# 	IF (BUILD_RT_ANY)
# 	   SUBDIRS(rt_any)
# 	ENDIF (BUILD_RT_ANY)

OPTION(BUILD_RT_DEBUG "Build rt_debug? (Routing extension for debugging the routing module)" OFF)
 	IF (BUILD_RT_DEBUG)
 	   SUBDIRS(rt_debug)
 	ENDIF (BUILD_RT_DEBUG)

OPTION(BUILD_APP_TEST "Build app_test? (Testing application to send dummy message to another peer, for testing purpose only)" OFF)
 	IF (BUILD_APP_TEST)
 	   SUBDIRS(app_test)
 	ENDIF (BUILD_APP_TEST)
