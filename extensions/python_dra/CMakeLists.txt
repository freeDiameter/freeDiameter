# The python_dra extension
PROJECT("Extension that allows routing score and avp changes from within a python script" C)

# List of source files
SET(PYTHON_DRA_SRC
	python_dra.c
	python_dra_config.c
)

# Compile as a module
FD_ADD_EXTENSION(python_dra ${PYTHON_DRA_SRC} ${PY_LIB})
TARGET_LINK_LIBRARIES(python_dra -lpython3.10)

####
## INSTALL section ##

# We install with the daemon component because it is a base feature.
INSTALL(TARGETS python_dra
	LIBRARY DESTINATION ${INSTALL_EXTENSIONS_SUFFIX}
	COMPONENT freeDiameter-daemon)