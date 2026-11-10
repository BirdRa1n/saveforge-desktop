qt_add_library(saveforge_archive STATIC
    include/saveforge/archive/Gamesave.h
    Gamesave.cpp
)
add_library(saveforge::archive ALIAS saveforge_archive)

target_include_directories(saveforge_archive PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
# The archive embeds vault snapshot documents (gamesave/SPEC.md), so it reads
# the vault's types; it never touches a vault on disk.
target_link_libraries(saveforge_archive
    PUBLIC saveforge::core saveforge::platform saveforge::profiles saveforge::vault Qt6::Core
    PRIVATE saveforge::miniz
)
saveforge_set_warnings(saveforge_archive)
