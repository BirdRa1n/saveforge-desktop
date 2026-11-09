qt_add_library(saveforge_registry STATIC
    include/saveforge/registry/Ed25519.h
    Ed25519.cpp
)
add_library(saveforge::registry ALIAS saveforge_registry)

target_include_directories(saveforge_registry PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(saveforge_registry
    PUBLIC saveforge::core saveforge::profiles Qt6::Core
    PRIVATE saveforge::monocypher saveforge::zstd
)
saveforge_set_warnings(saveforge_registry)
