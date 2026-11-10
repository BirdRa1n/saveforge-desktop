qt_add_library(savevault_archive STATIC
    include/savevault/archive/Gamesave.h
    Gamesave.cpp
)
add_library(savevault::archive ALIAS savevault_archive)

target_include_directories(savevault_archive PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
# The archive embeds vault snapshot documents (gamesave/SPEC.md), so it reads
# the vault's types; it never touches a vault on disk.
target_link_libraries(savevault_archive
    PUBLIC savevault::core savevault::platform savevault::profiles savevault::vault Qt6::Core
    PRIVATE savevault::miniz
)
savevault_set_warnings(savevault_archive)
