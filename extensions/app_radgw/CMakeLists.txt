# The app_radgw extension
PROJECT("RADIUS/Diameter extensible gateway application for freeDiameter" C)


# Overwrite the debug level for the extension code if configured
OPTION(DEBUG_LEVEL_APP_RADGW "Overwrite debug level for the extension app_radgw if defined" OFF)
IF (DEBUG_LEVEL_APP_RADGW)
     ADD_DEFINITIONS(-DTRACE_LEVEL=${DEBUG_LEVEL_APP_RADGW})
ENDIF (DEBUG_LEVEL_APP_RADGW)


########### Utility library #############
# utility libray for the extension and its plugins
# See rgw_common.h for detail

SET( RG_COMMON_SRC
	radius.c
	md5.c
	rgw_msg_codes.c
	rgw_msg_attrtype.c
)
SET( RG_COMMON_HEADER
	rgw_common.h
	radius.h
	md5.h
	hostap_compat.h
)
ADD_LIBRARY(rgw_common ${RG_COMMON_SRC})



########### Main app_radgw extension #############

# Parser files
BISON_FILE(rgw_conf.y)
FLEX_FILE(rgw_conf.l)
SET_SOURCE_FILES_PROPERTIES(lex.rgw_conf.c rgw_conf.tab.c PROPERTIES COMPILE_FLAGS "-I ${CMAKE_CURRENT_SOURCE_DIR}")

# List of source files
SET( RGW_DEFAULT_SRC
	rgw_main.c
	lex.rgw_conf.c
	rgw_conf.tab.c
	rgw_conf.tab.h
	rgw_clients.c
	rgw_plugins.c
	rgw_msg.c
	rgw_servers.c
	rgw_worker.c
)

# Compile these files as a freeDiameter extension.
FD_ADD_EXTENSION(app_radgw ${RGW_DEFAULT_SRC} ${RG_COMMON_HEADER})
TARGET_LINK_LIBRARIES(app_radgw rgw_common)



########### RADIUS/Diameter translation agent plugins (support for RADIUS protocol) ############
# Use the macro RGWX_ADD_PLUGIN(name files...) to create a plugin.
# It is equivalent to add_library with the appropriate parameters
# and naming conventions (.rgwx : Radius GateWay eXtension)
MACRO(RGWX_ADD_PLUGIN PLGNAME)
  ADD_LIBRARY(${PLGNAME} MODULE ${ARGN})
  SET_TARGET_PROPERTIES(${PLGNAME} PROPERTIES PREFIX "" )
  SET_TARGET_PROPERTIES(${PLGNAME} PROPERTIES SUFFIX ".rgwx" )
  TARGET_LINK_LIBRARIES(${PLGNAME} rgw_common)
ENDMACRO(RGWX_ADD_PLUGIN)


### Debug 
# Example of plugin:
OPTION(BUILD_RGWX_SAMPLE "Build sample plugin? (for developers only)" OFF)
 	IF (BUILD_RGWX_SAMPLE)
 	   RGWX_ADD_PLUGIN(sample ${RG_COMMON_HEADER} rgwx_sample.c)
 	ENDIF (BUILD_RGWX_SAMPLE)

# A plugin for debug: dumps RADIUS and Diameter messages state at the time the plugin is called.
OPTION(BUILD_RGWX_DEBUG "Build debug plugin? (display status of RADIUS and Diameter messages)" ON)
 	IF (BUILD_RGWX_DEBUG)
 	   RGWX_ADD_PLUGIN(debug ${RG_COMMON_HEADER} rgwx_debug.c)
 	ENDIF (BUILD_RGWX_DEBUG)


### Authentication, Authorization messages translation.
OPTION(BUILD_RGWX_AUTH "Build Authentication & Authorization RADIUS translation plugin? (RFC2865, RFC3579)" ON)
	IF (BUILD_RGWX_AUTH)
 	   RGWX_ADD_PLUGIN(auth ${RG_COMMON_HEADER} rgwx_auth.c)
	ENDIF (BUILD_RGWX_AUTH)








# 
# OPTION(BUILD_SUB_ECHO_DROP "Build 'echo/drop' sub-extension? (echo or drop specific RADIUS attributes, no Diameter translation)" ON)
#  	IF (BUILD_SUB_ECHO_DROP)
# 	   ADD_DEFINITIONS(-DSUB_ECHO_DROP_VERBO=2)
#  	   BISON_FILE(sub_echo_drop.y)
#  	   FLEX_FILE(sub_echo_drop.l)
#  	   SET_SOURCE_FILES_PROPERTIES(lex.sub_echo_drop.c sub_echo_drop.tab.c PROPERTIES COMPILE_FLAGS "-I ${CMAKE_CURRENT_SOURCE_DIR}")
#  	   ADD_LIBRARY(sub_echo_drop MODULE ${RG_COMMON_HEADER}
# 	   	sub_echo_drop.c
# 		sub_echo_drop.h
# 		lex.sub_echo_drop.c
# 		sub_echo_drop.tab.c
# 		sub_echo_drop.tab.h)
# 	   TARGET_LINK_LIBRARIES(sub_echo_drop rg_common)
#  	ENDIF (BUILD_SUB_ECHO_DROP)
# 
# OPTION(BUILD_SUB_ACCT "Build RADIUS Accounting sub-extension? (RFC2866)" ON)
#  	IF (BUILD_SUB_ACCT)
# 	   ADD_DEFINITIONS(-DSUB_ACCT_VERBO=2)
#  	   ADD_LIBRARY(sub_acct MODULE ${RG_COMMON_HEADER} sub_acct.c)
# 	   TARGET_LINK_LIBRARIES(sub_acct rg_common)
#  	ENDIF (BUILD_SUB_ACCT)
