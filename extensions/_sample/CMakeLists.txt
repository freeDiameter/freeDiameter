# The sample extension
PROJECT("Sample extension" C)

# Compile as a module
ADD_LIBRARY(sample MODULE sample.c fini.c)

# Comply to naming scheme
SET_TARGET_PROPERTIES(sample PROPERTIES PREFIX "${FD_EXT_PREFIX}" )
SET_TARGET_PROPERTIES(sample PROPERTIES SUFFIX "${FD_EXT_SUFFIX}" )
