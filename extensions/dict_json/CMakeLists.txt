# The dict_json extension
PROJECT("JSON dictionary files support" C)

INCLUDE(FindPkgConfig)

# We use JSONCPP and JSON-Schema to parse and validate JSON files
PKG_CHECK_MODULES(JSONCPP REQUIRED jsoncpp)
# https://github.com/nfotex/json-schema/
PKG_CHECK_MODULES(JSON_SCHEMA REQUIRED json-schema)
PKG_CHECK_MODULES(PCRECPP REQUIRED libpcrecpp)

# List of source files
SET(DICT_JSON_SRC
	dict_json.cc
	${CMAKE_CURRENT_BINARY_DIR}/dict_json_dict_schema.cc
)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
INCLUDE_DIRECTORIES(${JSONCPP_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${JSON_SCHEMA_INCLUDE_DIR})

LINK_DIRECTORIES(${JSONCPP_LIBDIR})
LINK_DIRECTORIES(${JSON_SCHEMA_LIBDIR})

# Compile these files as a freeDiameter extension
FD_ADD_EXTENSION(dict_json ${DICT_JSON_SRC})

TARGET_LINK_LIBRARIES(dict_json ${JSONCPP_LIBRARIES} ${JSON_SCHEMA_STATIC_LIBRARIES})

ADD_EXECUTABLE(dict-json-diff dict-json-diff.cc)
TARGET_LINK_LIBRARIES(dict-json-diff ${JSONCPP_LIBRARIES} ${JSON_SCHEMA_STATIC_LIBRARIES})

ADD_EXECUTABLE(json-schema-to-c json-schema-to-c.cc)
TARGET_LINK_LIBRARIES(json-schema-to-c ${JSONCPP_LIBRARIES} ${JSON_SCHEMA_STATIC_LIBRARIES} ${PCRECPP_LIBRARIES})

ADD_CUSTOM_COMMAND(
	OUTPUT dict_json_dict_schema.cc
	COMMAND json-schema-to-c ${CMAKE_CURRENT_SOURCE_DIR}/dict_json_dict_schema.json ${CMAKE_CURRENT_BINARY_DIR}/dict_json_dict_schema.cc
	DEPENDS dict_json_dict_schema.json
)

####
## INSTALL section ##

INSTALL(TARGETS dict_json
	LIBRARY DESTINATION ${INSTALL_EXTENSIONS_SUFFIX}
	COMPONENT freeDiameter-dictionary-json)
INSTALL(TARGETS dict-json-diff
	RUNTIME DESTINATION ${INSTALL_DAEMON_SUFFIX}
	COMPONENT freeDiameter-dictionary-json)
