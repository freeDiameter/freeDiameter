# - Try to find GNU TLS library and headers
# Once done, this will define
#
#  GNUTLS_FOUND - system has GNU TLS
#  GNUTLS_INCLUDE_DIRS - the GNU TLS include directories
#  GNUTLS_LIBRARIES - link these to use GNU TLS

include(LibFindMacros)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(GNUTLS_PKGCONF gnutls)

# Include dir
find_path(GNUTLS_INCLUDE_DIR
  NAMES gnutls/gnutls.h
  PATHS ${GNUTLS_PKGCONF_INCLUDE_DIRS}
)

# Finally the library itself
find_library(GNUTLS_LIBRARY
  NAMES gnutls
  PATHS ${GNUTLS_PKGCONF_LIBRARY_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(GNUTLS_PROCESS_INCLUDES GNUTLS_INCLUDE_DIR)
set(GNUTLS_PROCESS_LIBS GNUTLS_LIBRARY)
libfind_process(GNUTLS)
