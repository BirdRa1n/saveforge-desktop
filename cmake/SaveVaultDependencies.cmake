# Third-party dependencies other than Qt.
#
# Each dependency is looked up on the system first (distribution packages,
# Flatpak SDKs, a vcpkg toolchain if the developer uses one). When it is not
# found, CMake downloads a pinned release and verifies its SHA-256 before
# building it, so builds are reproducible and need no extra setup.
# Rationale: docs/adr/0001-third-party-dependencies.md.

include(FetchContent)

# --- zstd: compression of vault blobs -------------------------------------
# Release: https://github.com/facebook/zstd/releases/tag/v1.5.7
# SHA-256 matches the zstd-1.5.7.tar.gz.sha256 file published with the release.
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
set(ZSTD_MULTITHREAD_SUPPORT OFF CACHE BOOL "" FORCE)

FetchContent_Declare(zstd
    URL https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz
    URL_HASH SHA256=eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3
    SOURCE_SUBDIR build/cmake
    FIND_PACKAGE_ARGS 1.5 CONFIG
)
FetchContent_MakeAvailable(zstd)

# savevault::zstd hides whether zstd came from the system or was built here.
add_library(savevault_zstd INTERFACE)
add_library(savevault::zstd ALIAS savevault_zstd)
if(TARGET zstd::libzstd_static)
    target_link_libraries(savevault_zstd INTERFACE zstd::libzstd_static)
elseif(TARGET zstd::libzstd_shared)
    target_link_libraries(savevault_zstd INTERFACE zstd::libzstd_shared)
elseif(TARGET zstd::libzstd)
    target_link_libraries(savevault_zstd INTERFACE zstd::libzstd)
elseif(TARGET libzstd_static)
    target_link_libraries(savevault_zstd INTERFACE libzstd_static)
    target_include_directories(savevault_zstd SYSTEM INTERFACE "${zstd_SOURCE_DIR}/lib")
else()
    message(FATAL_ERROR "zstd was neither found nor built")
endif()

# --- Monocypher: Ed25519 verification of registry signatures ---------------
# Release: https://github.com/LoupVaillant/Monocypher/releases/tag/4.0.3
# SHA-512 matches https://monocypher.org/download/monocypher-4.0.3.tar.gz.sha512.
# Monocypher ships no CMake package, so it is always built from the pinned
# source: two C files (core + the optional SHA-512 based Ed25519).
FetchContent_Declare(monocypher
    URL https://github.com/LoupVaillant/Monocypher/releases/download/4.0.3/monocypher-4.0.3.tar.gz
    URL_HASH SHA512=40904ada5c7ee4f7741733e38b69a30a4b0561cbffba5ffe7c2dce16136d540251ec0d9056ff606510d3b5b708fb8a40db7e0870d4a0b2dc17ba2bfb880f8965
)
FetchContent_MakeAvailable(monocypher)

enable_language(C)
add_library(savevault_monocypher STATIC
    "${monocypher_SOURCE_DIR}/src/monocypher.c"
    "${monocypher_SOURCE_DIR}/src/optional/monocypher-ed25519.c"
)
add_library(savevault::monocypher ALIAS savevault_monocypher)
target_include_directories(savevault_monocypher SYSTEM PUBLIC
    "${monocypher_SOURCE_DIR}/src"
    "${monocypher_SOURCE_DIR}/src/optional"
)
set_target_properties(savevault_monocypher PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)

# --- miniz: ZIP container of .gamesave exports -----------------------------
# Release: https://github.com/richgel999/miniz/releases/tag/3.1.2
# SHA-256 matches the digest GitHub publishes for the release asset.
# The release asset is the single-file amalgamation (miniz.c + miniz.h), built
# here as a static library; zstd stays the vault's compressor.
FetchContent_Declare(miniz
    URL https://github.com/richgel999/miniz/releases/download/3.1.2/miniz-3.1.2.zip
    URL_HASH SHA256=f0446d863f9c19926ad9483c523fdc42e42b8d4a6a431d27e09d49c79a140d9a
)
# The archive has no CMake project, so this only downloads and unpacks it.
FetchContent_MakeAvailable(miniz)

add_library(savevault_miniz STATIC "${miniz_SOURCE_DIR}/miniz.c")
add_library(savevault::miniz ALIAS savevault_miniz)
target_include_directories(savevault_miniz SYSTEM PUBLIC "${miniz_SOURCE_DIR}")
# Keep zlib's names free in case a system zlib is linked into the same binary.
target_compile_definitions(savevault_miniz PUBLIC MINIZ_NO_ZLIB_COMPATIBLE_NAMES)
set_target_properties(savevault_miniz PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)
