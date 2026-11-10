qt_add_library(savevault_vault STATIC
    include/savevault/vault/BlobStore.h
    include/savevault/vault/Snapshot.h
    include/savevault/vault/Vault.h
    BlobStore.cpp
    Snapshot.cpp
    Vault.cpp
)
add_library(savevault::vault ALIAS savevault_vault)

target_include_directories(savevault_vault PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(savevault_vault
    PUBLIC savevault::core savevault::profiles Qt6::Core
    PRIVATE savevault::zstd
)
savevault_set_warnings(savevault_vault)
