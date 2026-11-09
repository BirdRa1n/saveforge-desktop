# Read by CPack once per generator (CPACK_PROJECT_CONFIG_FILE), after the
# settings of SaveForgePackaging.cmake.

if(CPACK_GENERATOR STREQUAL "INNOSETUP")
    # Tell the installer apart from the portable zip of the same build.
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}-setup")
    # The top-level folder is for archives; the installer puts the files
    # straight into the directory the user picks.
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
    set(CPACK_COMPONENT_INCLUDE_TOPLEVEL_DIRECTORY OFF)
endif()
