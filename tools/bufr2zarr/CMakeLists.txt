# copy python file bufr2zarr.py to bin directory
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/bufr2zarr.py
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Zarr
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE
                    WORLD_READ WORLD_EXECUTE)

