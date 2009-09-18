PROJECT("freeDiameter extensions" C)

# We want all resulting objects in the same folder
SET(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})
# And named fdext_XXX instead of libXXX.so

#-- faire une macro ici!
SET(FD_EXT_PREFIX "fdext_" PARENT_SCOPE)
SET(FD_EXT_SUFFIX "" PARENT_SCOPE)

###########################
# Extensions section

####
# Diameter applications dictionary
# OPTION(BUILD_DICT_NASREQ "Build NASREQ (RFC4005) Dictionary definitions?" ON)
# 	IF (BUILD_DICT_NASREQ)
# 	   SUBDIRS(dict_nasreq)
# 	ENDIF (BUILD_DICT_NASREQ)

# OPTION(BUILD_DICT_EAP "Build Diameter EAP (RFC4072) Dictionary definitions?" ON)
# 	IF (BUILD_DICT_EAP)
# 	   SUBDIRS(dict_eap)
# 	ENDIF (BUILD_DICT_EAP)



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

OPTION(BUILD_SAMPLE "Build sample? (Simple extension to demonstrate extension mechanism, for developpers only)" OFF)
	IF (BUILD_SAMPLE)
	   SUBDIRS(_sample)
	ENDIF (BUILD_SAMPLE)

# OPTION(BUILD_RT_ANY "Build rt_any? (Routing extension sending message to any peer available, for testing purpose only)" OFF)
# 	IF (BUILD_RT_ANY)
# 	   SUBDIRS(rt_any)
# 	ENDIF (BUILD_RT_ANY)

# OPTION(BUILD_RT_DEBUG "Build rt_debug? (Routing extension tracing routing list, for debug purpose only)" OFF)
# 	IF (BUILD_RT_DEBUG)
# 	   SUBDIRS(rt_debug)
# 	ENDIF (BUILD_RT_DEBUG)

# OPTION(BUILD_APP_TEST "Build app_test? (Testing application to send dummy message to another peer, for testing purpose only)" OFF)
# 	IF (BUILD_APP_TEST)
# 	   SUBDIRS(app_test)
# 	ENDIF (BUILD_APP_TEST)
