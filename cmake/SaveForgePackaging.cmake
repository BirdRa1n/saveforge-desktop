# Packages built with CPack from the install tree (`cmake --install` plus the
# Qt deploy step of src/app). Unsigned for now (roadmap D1.11); code signing
# comes with D2.7.
#
#   Windows  SaveForge-<version>-windows-x64-setup.exe  installer (Inno Setup)
#            SaveForge-<version>-windows-x64.zip   portable: unzip and run bin/SaveForge.exe
#   Linux    SaveForge-<version>-linux-x64.tar.gz  run bin/SaveForge
#   macOS    SaveForge-<version>-macos-<arch>.dmg  drag SaveForge.app to Applications
#
# Every install rule of the app belongs to the "SaveForge" component.
#
# Usage, from a configured release build directory:
#   cmake --build . && cpack
#
# SAVEFORGE_PACKAGE_VERSION overrides the version in the file name, e.g. for
# pre-releases ("0.1.0-alpha.1"); the project version itself stays X.Y.Z.

set(SAVEFORGE_PACKAGE_VERSION "${PROJECT_VERSION}" CACHE STRING "Version shown in package file names")

if(WIN32)
    set(_os windows)
    set(CPACK_GENERATOR ZIP)
    # The installer needs Inno Setup 6 (ISCC). Without it, only the zip is built.
    set(_pf86 "ProgramFiles(x86)")
    find_program(SAVEFORGE_ISCC ISCC
        HINTS "$ENV{${_pf86}}/Inno Setup 6" "$ENV{ProgramFiles}/Inno Setup 6" "$ENV{LOCALAPPDATA}/Programs/Inno Setup 6")
    if(SAVEFORGE_ISCC)
        list(PREPEND CPACK_GENERATOR INNOSETUP)
        set(CPACK_INNOSETUP_EXECUTABLE "${SAVEFORGE_ISCC}")
    else()
        message(STATUS "Inno Setup not found: cpack builds the portable zip only")
    endif()
elseif(APPLE)
    set(_os macos)
    set(CPACK_GENERATOR DragNDrop)
else()
    set(_os linux)
    set(CPACK_GENERATOR TGZ)
endif()

if(APPLE AND CMAKE_OSX_ARCHITECTURES)
    string(REPLACE ";" "-" _arch "${CMAKE_OSX_ARCHITECTURES}")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|x86_64|x64)$")
    set(_arch x64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64|ARM64)$")
    set(_arch arm64)
else()
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _arch)
endif()

# Values are written to CPackConfig.cmake escaped, so backslashes (Inno Setup
# paths) and quotes survive.
set(CPACK_VERBATIM_VARIABLES ON)
set(CPACK_PACKAGE_NAME "SaveForge")
set(CPACK_PACKAGE_VENDOR "SaveForge")
set(CPACK_PACKAGE_VERSION "${SAVEFORGE_PACKAGE_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_PACKAGE_FILE_NAME "SaveForge-${SAVEFORGE_PACKAGE_VERSION}-${_os}-${_arch}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "SaveForge")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_STRIP_FILES ON)
# One top-level folder in archives, so unpacking never scatters files. Disk
# images need their content at the root (SaveForge.app next to the
# Applications link), and DragNDrop fails with a top-level folder.
if(NOT APPLE)
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
    set(CPACK_COMPONENT_INCLUDE_TOPLEVEL_DIRECTORY ON)
endif()
set(CPACK_DMG_VOLUME_NAME "SaveForge ${SAVEFORGE_PACKAGE_VERSION}")
set(CPACK_PACKAGE_CHECKSUM SHA256)

# MSVC builds need the Visual C++ runtime DLLs next to the executables:
# Qt's deploy step does not copy them, and many machines lack them.
if(MSVC)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "${CMAKE_INSTALL_BINDIR}")
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT SaveForge)
    include(InstallRequiredSystemLibraries)
endif()

# Next to SaveForge.app in the disk image; under share/doc elsewhere.
if(APPLE)
    set(_docdir .)
else()
    set(_docdir "${CMAKE_INSTALL_DOCDIR}")
endif()
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_SOURCE_DIR}/README.md" DESTINATION "${_docdir}"
    COMPONENT SaveForge)

# Only SaveForge's own install rules: third-party projects built through
# FetchContent (zstd) add headers and static libraries nobody needs at run time.
set(CPACK_COMPONENTS_ALL SaveForge)
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_DMG_COMPONENT_INSTALL ON)

# --- Windows installer (Inno Setup) ------------------------------------------
# Installs for the current user without administrator rights by default (the
# wizard offers "all users" too), adds Start menu and optional desktop
# shortcuts, an uninstaller, and runs the app at the end. AppId identifies the
# app across versions, so a new installer upgrades the old one: never change it.
set(CPACK_PACKAGE_EXECUTABLES SaveForge SaveForge)
set(CPACK_CREATE_DESKTOP_LINKS SaveForge)
set(CPACK_INNOSETUP_RUN_EXECUTABLES SaveForge)
set(CPACK_INNOSETUP_PROGRAM_MENU_FOLDER .)
set(CPACK_INNOSETUP_LANGUAGES brazilianPortuguese english)
set(CPACK_INNOSETUP_USE_MODERN_WIZARD ON)
set(CPACK_INNOSETUP_ICON_FILE "${PROJECT_SOURCE_DIR}/src/app/icons/saveforge.ico")
set(CPACK_INNOSETUP_SETUP_AppId "{{D02782E0-D807-4590-BD2E-2DFA33ED8B6B}")
set(CPACK_INNOSETUP_SETUP_PrivilegesRequired lowest)
set(CPACK_INNOSETUP_SETUP_PrivilegesRequiredOverridesAllowed dialog)
set(CPACK_INNOSETUP_SETUP_UninstallDisplayIcon "{app}\\bin\\SaveForge.exe")
set(CPACK_INNOSETUP_SETUP_UninstallDisplayName "SaveForge")
set(CPACK_INNOSETUP_SETUP_CloseApplications yes)
set(CPACK_INNOSETUP_SETUP_DisableProgramGroupPage yes)
# The only component is required and hidden: no component page in the wizard.
set(CPACK_COMPONENT_SAVEFORGE_DISPLAY_NAME "SaveForge")
set(CPACK_COMPONENT_SAVEFORGE_REQUIRED ON)
set(CPACK_COMPONENT_SAVEFORGE_HIDDEN ON)
set(CPACK_INNOSETUP_COMPONENT_INSTALL ON)

# Per-generator settings (the installer's file name) live in this file.
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_CURRENT_LIST_DIR}/SaveForgeCPackOptions.cmake")

include(CPack)
