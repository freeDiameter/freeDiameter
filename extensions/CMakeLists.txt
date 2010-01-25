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

OPTION(BUILD_ACL_WL "Build acl_wl? (White-list of remote connecting peers)" ON)
	IF (BUILD_ACL_WL)
		SUBDIRS(acl_wl)
 	ENDIF (BUILD_ACL_WL)


####
# Debug / test extensions

OPTION(BUILD_SAMPLE "Build sample.fdx? (Simple extension to demonstrate extension mechanism, for developpers only)" OFF)
	IF (BUILD_SAMPLE)
	   SUBDIRS(_sample)
	ENDIF (BUILD_SAMPLE)

OPTION(BUILD_DBG_MONITOR "Build dbg_monitor.fdx? (display periodical debug information on the console)" OFF)
	IF (BUILD_DBG_MONITOR)
	   SUBDIRS(dbg_monitor)
	ENDIF (BUILD_DBG_MONITOR)

OPTION(BUILD_DBG_RT "Build dbg_rt.fdx? (Routing extension for debugging the routing module)" OFF)
 	IF (BUILD_DBG_RT)
 	   SUBDIRS(dbg_rt)
 	ENDIF (BUILD_DBG_RT)

OPTION(BUILD_TEST_APP "Build test_app.fdx? (Testing application to send dummy message to another peer, for testing purpose only)" OFF)
 	IF (BUILD_TEST_APP)
 	   SUBDIRS(test_app)
 	ENDIF (BUILD_TEST_APP)

OPTION(BUILD_TEST_RT_ANY "Build test_rt_any.fdx? (Routing extension randomly sending message to any peer available, for testing purpose only)" OFF)
 	IF (BUILD_TEST_RT_ANY)
 	   SUBDIRS(test_rt_any)
 	ENDIF (BUILD_TEST_RT_ANY)
