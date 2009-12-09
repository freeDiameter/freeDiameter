# The rt_debug extension
PROJECT("Random routing extension (for tests)" C)

# Compile as a module
FD_ADD_EXTENSION(test_rt_any test_rt_any.c)
