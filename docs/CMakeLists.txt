include(GNUInstallDirs)

# Only useful for make install/package under *nix OSes, but not macOS.
if ( UNIX AND NOT APPLE )
    INSTALL(FILES openxcom.6 DESTINATION ${CMAKE_INSTALL_MANDIR}/man6)
endif ()
