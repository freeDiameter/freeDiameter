PROJECT("Dictionary dump to JSON extension" C)
FD_ADD_EXTENSION(dbg_dict_dump_json dbg_dict_dump_json.cc)

# sneakily uses dictionary-internal.h
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../../libfdproto)

INCLUDE(FindPkgConfig)

# Uses JSONCPP to write JSON files
PKG_CHECK_MODULES(JSONCPP REQUIRED jsoncpp)
INCLUDE_DIRECTORIES(${JSONCPP_INCLUDE_DIRS})
LINK_DIRECTORIES(${JSONCPP_LIBDIR})
TARGET_LINK_LIBRARIES(dbg_dict_dump_json ${JSONCPP_LIBRARIES})

####
## INSTALL section ##

INSTALL(TARGETS dbg_dict_dump_json
	LIBRARY DESTINATION ${INSTALL_EXTENSIONS_SUFFIX}
	COMPONENT freeDiameter-debug-tools)

