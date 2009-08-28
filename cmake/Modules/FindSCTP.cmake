# - Try to find SCTP library and headers
# Once done, this will define
#
#  SCTP_FOUND - system has SCTP
#  SCTP_INCLUDE_DIRS - the SCTP include directories
#  SCTP_LIBRARIES - link these to use SCTP

include(LibFindMacros)

# Use pkg-config to get hints about paths (Note: not yet supported?)
# libfind_pkg_check_modules(SCTP_PKGCONF sctp)

# Include dir
find_path(SCTP_INCLUDE_DIR
  NAMES netinet/sctp.h
  PATHS ${SCTP_PKGCONF_INCLUDE_DIRS}
)

# Finally the library itself
find_library(SCTP_LIBRARY
  NAMES sctp
  PATHS ${SCTP_PKGCONF_LIBRARY_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(SCTP_PROCESS_INCLUDES SCTP_INCLUDE_DIR)
set(SCTP_PROCESS_LIBS SCTP_LIBRARY)
libfind_process(SCTP)
