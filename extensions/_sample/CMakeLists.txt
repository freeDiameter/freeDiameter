# The sample extension
PROJECT("Sample extension" C)

# Compile as a module
FD_ADD_EXTENSION(dbg_sample sample.c fini.c)
