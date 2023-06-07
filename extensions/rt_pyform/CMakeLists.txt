# The rt_pyform extension
PROJECT("Routing extension that transforms AVP values using a python script" C)

# List of source files
SET(RT_PYFORM_SRC
	rt_pyform.c
	rt_pyform_config.c
	pyformer.c
)

# Compile as a module
FD_ADD_EXTENSION(rt_pyform ${RT_PYFORM_SRC} ${PY_LIB})
TARGET_LINK_LIBRARIES(rt_pyform -lpython3.10)