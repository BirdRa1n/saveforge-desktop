qt_add_library(saveforge_registry STATIC
    include/saveforge/registry/Channel.h
    include/saveforge/registry/Ed25519.h
    include/saveforge/registry/Fetcher.h
    include/saveforge/registry/Metadata.h
    include/saveforge/registry/ProfileCatalog.h
    include/saveforge/registry/RegistryCache.h
    include/saveforge/registry/RegistryUpdater.h
    Channel.cpp
    Ed25519.cpp
    Fetcher.cpp
    Metadata.cpp
    ProfileCatalog.cpp
    RegistryCache.cpp
    RegistryUpdater.cpp
)
add_library(saveforge::registry ALIAS saveforge_registry)

target_include_directories(saveforge_registry PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(saveforge_registry
    PUBLIC saveforge::core saveforge::platform saveforge::profiles Qt6::Core
    PRIVATE Qt6::Network saveforge::monocypher saveforge::zstd
)
saveforge_set_warnings(saveforge_registry)

# Trust anchor: root metadata (version 1) whose root keys sign every registry
# distribution this build accepts. Until production keys exist, the default is
# the PUBLIC test root of saveforge-specs, which anyone can sign with.
set(SAVEFORGE_REGISTRY_TRUST_ANCHOR
    "${SAVEFORGE_SPECS_DIR}/registry/examples/root.v1/valid/root.json"
    CACHE FILEPATH "Root metadata embedded as the registry trust anchor")
if(SAVEFORGE_REGISTRY_TRUST_ANCHOR STREQUAL "${SAVEFORGE_SPECS_DIR}/registry/examples/root.v1/valid/root.json")
    message(WARNING
        "The registry trust anchor is the PUBLIC test key from saveforge-specs. "
        "Fine for development; release builds must set SAVEFORGE_REGISTRY_TRUST_ANCHOR.")
    target_compile_definitions(saveforge_registry PRIVATE SAVEFORGE_REGISTRY_TEST_ANCHOR)
endif()
set_source_files_properties("${SAVEFORGE_REGISTRY_TRUST_ANCHOR}" PROPERTIES QT_RESOURCE_ALIAS trust-anchor.json)
qt_add_resources(saveforge_registry saveforge_registry_anchor
    PREFIX "/saveforge/registry"
    FILES "${SAVEFORGE_REGISTRY_TRUST_ANCHOR}"
)

# Mirrors tried in order when the app updates its profiles, separated by ";".
# Empty means the app only uses the bundled distribution and manual imports.
set(SAVEFORGE_REGISTRY_MIRRORS "" CACHE STRING "Registry mirror base URLs, separated by semicolons")
if(SAVEFORGE_REGISTRY_MIRRORS)
    target_compile_definitions(saveforge_registry PRIVATE "SAVEFORGE_REGISTRY_MIRRORS=\"${SAVEFORGE_REGISTRY_MIRRORS}\"")
endif()

# A registry distribution (the dist/v1 output of saveforge-registry) compiled
# into the app, so profiles are available on first run and offline.
set(SAVEFORGE_REGISTRY_BUNDLED_DIR "" CACHE PATH "Registry distribution embedded in the app")
if(SAVEFORGE_REGISTRY_BUNDLED_DIR)
    if(NOT EXISTS "${SAVEFORGE_REGISTRY_BUNDLED_DIR}/root.json")
        message(FATAL_ERROR "SAVEFORGE_REGISTRY_BUNDLED_DIR has no root.json: ${SAVEFORGE_REGISTRY_BUNDLED_DIR}")
    endif()
    file(GLOB_RECURSE _bundled RELATIVE "${SAVEFORGE_REGISTRY_BUNDLED_DIR}" CONFIGURE_DEPENDS
        "${SAVEFORGE_REGISTRY_BUNDLED_DIR}/*")
    set(_bundled_files "")
    foreach(_file IN LISTS _bundled)
        set_source_files_properties("${SAVEFORGE_REGISTRY_BUNDLED_DIR}/${_file}" PROPERTIES QT_RESOURCE_ALIAS "${_file}")
        list(APPEND _bundled_files "${SAVEFORGE_REGISTRY_BUNDLED_DIR}/${_file}")
    endforeach()
    qt_add_resources(saveforge_registry saveforge_registry_bundled
        PREFIX "/saveforge/registry/bundled"
        FILES ${_bundled_files}
    )
    message(STATUS "Bundling the registry distribution from ${SAVEFORGE_REGISTRY_BUNDLED_DIR}")
endif()
