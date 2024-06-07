# This file is called at build time. It regenerates the version.h file based on the git version.

if(GIT_EXECUTABLE)
  get_filename_component(SRC_DIR ${SRC} DIRECTORY)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --dirty
    WORKING_DIRECTORY ${SRC_DIR}
    OUTPUT_VARIABLE GIT_DESCRIBE_VERSION
    RESULT_VARIABLE GIT_DESCRIBE_ERROR_CODE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  if(NOT GIT_DESCRIBE_ERROR_CODE)
    set(FD_PROJECT_VERSION_GIT ${GIT_DESCRIBE_VERSION})
  endif()
endif()

if(NOT DEFINED FD_PROJECT_VERSION_GIT)
  set(FD_PROJECT_VERSION_GIT unknown)
  message(WARNING "Failed to determine FD_PROJECT_VERSION_GIT from Git tags. Using default version \"${FD_PROJECT_VERSION_GIT}\".")
endif()

configure_file(${SRC} ${DST})