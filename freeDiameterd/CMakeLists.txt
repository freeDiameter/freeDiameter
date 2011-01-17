# The subproject name
Project("freeDiameter simple daemon" C)

# Build the executable
ADD_EXECUTABLE(freeDiameterd main.c)

# The version
SET_TARGET_PROPERTIES(freeDiameterd PROPERTIES 
	VERSION ${FD_PROJECT_VERSION_MAJOR}.${FD_PROJECT_VERSION_MINOR}.${FD_PROJECT_VERSION_REV})

# The link command
TARGET_LINK_LIBRARIES(freeDiameterd libfdcore)

####
## INSTALL section ##

INSTALL(TARGETS freeDiameterd
	RUNTIME DESTINATION ${INSTALL_DAEMON_SUFFIX}
	COMPONENT freeDiameter-daemon)
