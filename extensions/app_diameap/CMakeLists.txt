# The Diameter EAP Application extension
PROJECT("Diameter EAP Application extension" C)

INCLUDE (CheckCSourceCompiles)

# Find MySQL 
FIND_PACKAGE(MySQL REQUIRED)
FIND_PACKAGE(Gcrypt REQUIRED)
INCLUDE_DIRECTORIES(${MySQL_INCLUDE_DIR})

# Check for my_bool, deprecated in MySQL 8.0
SET(CHECK_MYSQL_MY_BOOL "
#include <mysql.h>
int main() {
my_bool f;
}")
SET(CMAKE_REQUIRED_INCLUDES ${MySQL_INCLUDE_DIR})
SET(CMAKE_REQUIRED_LIBRARIES ${MySQL_LIBRARY})
CHECK_C_SOURCE_COMPILES("${CHECK_MYSQL_MY_BOOL}" HAVE_MYSQL_MY_BOOL)
IF (HAVE_MYSQL_MY_BOOL)
  ADD_DEFINITIONS(-DHAVE_MYSQL_MY_BOOL)
ENDIF()

# Parse plugins
ADD_SUBDIRECTORY(plugins)

# Parser 
BISON_FILE(diameap.y)
FLEX_FILE(diameap.l)
SET_SOURCE_FILES_PROPERTIES(lex.diameap.c diameap.tab.c PROPERTIES COMPILE_FLAGS "-I \"${CMAKE_CURRENT_SOURCE_DIR}\"")


set( diameapsrc
    diameap_defs.h
    diameap.h
    diameap_server.h
    diameap_plugins.h        
    diameap_eap.h
    plugins.h
    diameap_eappacket.h
    diameap_user.h
    diameap.tab.h
    diameap.tab.c
    lex.diameap.c        
    diameap.c
    diameap_server.c
    diameap_plugins.c
    diameap_init.c
    diameap_common.h
    diameap_eappacket.c
    libdiameap.h
    diameap_eap.c
    diameap_mysql.h
    diameap_mysql.c
    diameap_tls.h
    diameap_tls.c
    libcrypt.h
    libcrypt.c
    diameap_user.c
)

FD_ADD_EXTENSION(app_diameap ${diameapsrc})

TARGET_LINK_LIBRARIES(app_diameap ${GCRYPT_LIBRARY} ${GNUTLS_LIBRARIES} ${MySQL_LIBRARY} )

####
## INSTALL section ##

INSTALL(TARGETS app_diameap
	LIBRARY DESTINATION ${INSTALL_EXTENSIONS_SUFFIX}
	COMPONENT freeDiameter-eap-server)
