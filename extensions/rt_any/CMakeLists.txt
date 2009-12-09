# The rt_debug extension
PROJECT("Random routing extension (for tests)" C)

# Compile as a module
FD_ADD_EXTENSION(rt_any rt_any.c)
