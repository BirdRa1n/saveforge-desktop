# Packages built with CPack from the install tree (`cmake --install` plus the
# Qt deploy step of src/app). Unsigned for now (roadmap D1.11); code signing
# comes with D2.7.
#
#   Windows  SaveForge-<version>-windows-x64.zip   portable: unzip and run bin/SaveForge.exe
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

include(CPack)
