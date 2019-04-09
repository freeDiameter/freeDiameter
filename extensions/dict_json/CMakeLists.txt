# The dict_json extension
PROJECT("JSON dictionary files support" C)

INCLUDE(FindPkgConfig)

# We use JSONCPP and JSON-Schema to parse and validate JSON files
PKG_CHECK_MODULES(JSONCPP REQUIRED jsoncpp)
PKG_CHECK_MODULES(JSON_SCHEMA REQUIRED json-schema)

# List of source files
SET(DICT_JSON_SRC
	dict_json.cc
	dict_json_dict_schema.cc
)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
INCLUDE_DIRECTORIES(${JSONCPP_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${JSON_SCHEMA_INCLUDE_DIR})

LINK_DIRECTORIES(${JSONCPP_LIBDIR})
LINK_DIRECTORIES(${JSON_SCHEMA_LIBDIR})

# Compile these files as a freeDiameter extension
FD_ADD_EXTENSION(dict_json ${DICT_JSON_SRC})

TARGET_LINK_LIBRARIES(dict_json ${JSONCPP_LIBRARIES} ${JSON_SCHEMA_STATIC_LIBRARIES})

ADD_EXECUTABLE(dict-json-diff dict-json-diff.cc)
TARGET_LINK_LIBRARIES(dict-json-diff ${JSONCPP_LIBRARIES} ${JSON_SCHEMA_STATIC_LIBRARIES})


####
## INSTALL section ##

INSTALL(TARGETS dict_json
	LIBRARY DESTINATION ${INSTALL_EXTENSIONS_SUFFIX}
	COMPONENT freeDiameter-dictionary-json)
INSTALL(TARGETS dict-json-diff
	RUNTIME DESTINATION ${INSTALL_DAEMON_SUFFIX}
	COMPONENT freeDiameter-dictionary-json)

# dict_json_dict_schema.cc is created from dict_json_dict_schema.json
# the tool for that is not yet open source, but the conversion is straightforward
