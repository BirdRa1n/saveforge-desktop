qt_add_library(savevault_profiles STATIC
    include/savevault/profiles/PlaceholderCatalog.h
    include/savevault/profiles/PathRules.h
    include/savevault/profiles/Profile.h
    include/savevault/profiles/ProfileParser.h
    include/savevault/profiles/PathResolver.h
    PlaceholderCatalog.cpp
    PathRules.cpp
    Wildcard.h
    ProfileParser.cpp
    PathResolver.cpp
)
add_library(savevault::profiles ALIAS savevault_profiles)

target_include_directories(savevault_profiles PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(savevault_profiles PUBLIC savevault::core savevault::platform Qt6::Core)
savevault_set_warnings(savevault_profiles)

# Data files come straight from savevault-specs so the Desktop never carries a
# second copy of the placeholder catalog or the sensitive-location denylist.
qt_add_resources(savevault_profiles savevault_profiles_specs
    PREFIX "/savevault/specs"
    BASE "${SAVEVAULT_SPECS_DIR}"
    FILES
        "${SAVEVAULT_SPECS_DIR}/profile/placeholders.json"
        "${SAVEVAULT_SPECS_DIR}/profile/path-denylist.json"
)
