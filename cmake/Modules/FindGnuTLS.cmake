# - Find gnutls
# Find the native GNUTLS includes and library
#
#  GNUTLS_FOUND - True if gnutls found.
#  GNUTLS_INCLUDE_DIR - where to find gnutls.h, etc.
#  GNUTLS_LIBRARIES - List of libraries when using gnutls.
#  GNUTLS_NEW_VERSION - true if GnuTLS version is <= 2.10.0 (does not require additional separate gcrypt initialization)

if (GNUTLS_INCLUDE_DIR AND GNUTLS_LIBRARIES)
  set(GNUTLS_FIND_QUIETLY TRUE)
endif (GNUTLS_INCLUDE_DIR AND GNUTLS_LIBRARIES)

# Include dir
find_path(GNUTLS_INCLUDE_DIR
	NAMES
	  gnutls.h
	  gnutls/gnutls.h
)

# Library
find_library(GNUTLS_LIBRARY 
  NAMES gnutls
)

# handle the QUIETLY and REQUIRED arguments and set GNUTLS_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GNUTLS DEFAULT_MSG GNUTLS_LIBRARY GNUTLS_INCLUDE_DIR)

IF(GNUTLS_FOUND)
  SET( GNUTLS_LIBRARIES ${GNUTLS_LIBRARY} )
ELSE(GNUTLS_FOUND)
  SET( GNUTLS_LIBRARIES )
ENDIF(GNUTLS_FOUND)

# Lastly make it so that the GNUTLS_LIBRARY and GNUTLS_INCLUDE_DIR variables
# only show up under the advanced options in the gui cmake applications.
MARK_AS_ADVANCED( GNUTLS_LIBRARY GNUTLS_INCLUDE_DIR )

# Now check if the library is recent. gnutls_hash was added in 2.10.0.
IF( NOT( "${GNUTLS_VERSION_TEST_FOR}" STREQUAL "${GNUTLS_LIBRARY}" ))
  INCLUDE (CheckLibraryExists) 
  MESSAGE(STATUS "Rechecking GNUTLS_NEW_VERSION")
  UNSET(GNUTLS_NEW_VERSION)
  UNSET(GNUTLS_NEW_VERSION CACHE)
  GET_FILENAME_COMPONENT(GNUTLS_PATH ${GNUTLS_LIBRARY} PATH)
  CHECK_LIBRARY_EXISTS(gnutls gnutls_hash ${GNUTLS_PATH} GNUTLS_NEW_VERSION) 
  SET( GNUTLS_VERSION_TEST_FOR ${GNUTLS_LIBRARY} CACHE INTERNAL "Version the test was made against" )
ENDIF (NOT( "${GNUTLS_VERSION_TEST_FOR}" STREQUAL "${GNUTLS_LIBRARY}" ))
