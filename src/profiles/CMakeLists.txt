qt_add_library(saveforge_profiles STATIC
    include/saveforge/profiles/PlaceholderCatalog.h
    include/saveforge/profiles/PathRules.h
    PlaceholderCatalog.cpp
    PathRules.cpp
)
add_library(saveforge::profiles ALIAS saveforge_profiles)

target_include_directories(saveforge_profiles PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(saveforge_profiles PUBLIC saveforge::core saveforge::platform Qt6::Core)
saveforge_set_warnings(saveforge_profiles)

# Data files come straight from saveforge-specs so the Desktop never carries a
# second copy of the placeholder catalog or the sensitive-location denylist.
qt_add_resources(saveforge_profiles saveforge_profiles_specs
    PREFIX "/saveforge/specs"
    BASE "${SAVEFORGE_SPECS_DIR}"
    FILES
        "${SAVEFORGE_SPECS_DIR}/profile/placeholders.json"
        "${SAVEFORGE_SPECS_DIR}/profile/path-denylist.json"
)
