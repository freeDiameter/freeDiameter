# - Find gnutls
# Find the native GNUTLS includes and library
#
#  GNUTLS_FOUND - True if gnutls found.
#  GNUTLS_INCLUDE_DIR - where to find gnutls.h, etc.
#  GNUTLS_LIBRARIES - List of libraries when using gnutls.

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
