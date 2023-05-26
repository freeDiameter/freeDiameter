# Debug Peers Extension
PROJECT("Debug Peers Extension" C)
FD_ADD_EXTENSION(dbg_peers dbg_peers.c dbg_peers_config.c)


# Link against the shared libraries
target_link_libraries(dbg_peers
    /usr/lib/libprom.so
    /usr/lib/libpromhttp.so
)

# Specify the installation destination
install(TARGETS dbg_peers
    LIBRARY DESTINATION ${INSTALL_EXTENSIONS_SUFFIX}
    # LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${INSTALL_EXTENSIONS_SUFFIX}
    COMPONENT freeDiameter-debug-tools
)
