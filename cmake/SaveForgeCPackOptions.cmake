# Read by CPack once per generator (CPACK_PROJECT_CONFIG_FILE), after the
# settings of SaveForgePackaging.cmake.

# Tell the installer apart from the portable zip of the same build.
if(CPACK_GENERATOR STREQUAL "INNOSETUP")
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}-setup")
endif()
