qt_add_library(savevault_registry STATIC
    include/savevault/registry/Channel.h
    include/savevault/registry/Ed25519.h
    include/savevault/registry/Fetcher.h
    include/savevault/registry/JsonSpans.h
    include/savevault/registry/Metadata.h
    include/savevault/registry/ProfileCatalog.h
    include/savevault/registry/RegistryCache.h
    include/savevault/registry/RegistryUpdater.h
    Channel.cpp
    JsonSpans.cpp
    Ed25519.cpp
    Fetcher.cpp
    Metadata.cpp
    ProfileCatalog.cpp
    RegistryCache.cpp
    RegistryUpdater.cpp
)
add_library(savevault::registry ALIAS savevault_registry)

target_include_directories(savevault_registry PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(savevault_registry
    PUBLIC savevault::core savevault::platform savevault::profiles Qt6::Core
    PRIVATE Qt6::Network savevault::monocypher savevault::zstd
)
savevault_set_warnings(savevault_registry)

# Trust anchor: root metadata (version 1) whose root keys sign every registry
# distribution this build accepts. Until production keys exist, the default is
# the PUBLIC test root of savevault-specs, which anyone can sign with.
set(SAVEVAULT_REGISTRY_TRUST_ANCHOR
    "${SAVEVAULT_SPECS_DIR}/registry/examples/root.v1/valid/root.json"
    CACHE FILEPATH "Root metadata embedded as the registry trust anchor")
if(SAVEVAULT_REGISTRY_TRUST_ANCHOR STREQUAL "${SAVEVAULT_SPECS_DIR}/registry/examples/root.v1/valid/root.json")
    message(WARNING
        "The registry trust anchor is the PUBLIC test key from savevault-specs. "
        "Fine for development; release builds must set SAVEVAULT_REGISTRY_TRUST_ANCHOR.")
    target_compile_definitions(savevault_registry PRIVATE SAVEVAULT_REGISTRY_TEST_ANCHOR)
endif()
set_source_files_properties("${SAVEVAULT_REGISTRY_TRUST_ANCHOR}" PROPERTIES QT_RESOURCE_ALIAS trust-anchor.json)
qt_add_resources(savevault_registry savevault_registry_anchor
    PREFIX "/savevault/registry"
    FILES "${SAVEVAULT_REGISTRY_TRUST_ANCHOR}"
)

# Mirrors tried in order when the app updates its profiles, separated by ";".
# Empty means the app only uses the bundled distribution and manual imports.
# Development builds default to the test-key distribution that
# savevault-registry publishes on GitHub Pages; release builds name their own.
if(SAVEVAULT_REGISTRY_TRUST_ANCHOR STREQUAL "${SAVEVAULT_SPECS_DIR}/registry/examples/root.v1/valid/root.json")
    set(_default_mirrors "https://birdra1n.github.io/savevault-registry/v1/")
else()
    set(_default_mirrors "")
endif()
set(SAVEVAULT_REGISTRY_MIRRORS "${_default_mirrors}" CACHE STRING "Registry mirror base URLs, separated by semicolons")
if(SAVEVAULT_REGISTRY_MIRRORS)
    target_compile_definitions(savevault_registry PRIVATE "SAVEVAULT_REGISTRY_MIRRORS=\"${SAVEVAULT_REGISTRY_MIRRORS}\"")
endif()

# A registry distribution (the dist/v1 output of savevault-registry) compiled
# into the app, so profiles are available on first run and offline.
set(SAVEVAULT_REGISTRY_BUNDLED_DIR "" CACHE PATH "Registry distribution embedded in the app")
if(SAVEVAULT_REGISTRY_BUNDLED_DIR)
    if(NOT EXISTS "${SAVEVAULT_REGISTRY_BUNDLED_DIR}/root.json")
        message(FATAL_ERROR "SAVEVAULT_REGISTRY_BUNDLED_DIR has no root.json: ${SAVEVAULT_REGISTRY_BUNDLED_DIR}")
    endif()
    file(GLOB_RECURSE _bundled RELATIVE "${SAVEVAULT_REGISTRY_BUNDLED_DIR}" CONFIGURE_DEPENDS
        "${SAVEVAULT_REGISTRY_BUNDLED_DIR}/*")
    set(_bundled_files "")
    foreach(_file IN LISTS _bundled)
        set_source_files_properties("${SAVEVAULT_REGISTRY_BUNDLED_DIR}/${_file}" PROPERTIES QT_RESOURCE_ALIAS "${_file}")
        list(APPEND _bundled_files "${SAVEVAULT_REGISTRY_BUNDLED_DIR}/${_file}")
    endforeach()
    qt_add_resources(savevault_registry savevault_registry_bundled
        PREFIX "/savevault/registry/bundled"
        FILES ${_bundled_files}
    )
    message(STATUS "Bundling the registry distribution from ${SAVEVAULT_REGISTRY_BUNDLED_DIR}")
endif()
