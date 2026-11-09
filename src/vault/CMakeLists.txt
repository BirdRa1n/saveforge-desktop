qt_add_library(saveforge_vault STATIC
    include/saveforge/vault/BlobStore.h
    include/saveforge/vault/Snapshot.h
    include/saveforge/vault/Vault.h
    BlobStore.cpp
    FileSync.h
    FileSync.cpp
    Snapshot.cpp
    Vault.cpp
)
add_library(saveforge::vault ALIAS saveforge_vault)

target_include_directories(saveforge_vault PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(saveforge_vault
    PUBLIC saveforge::core saveforge::profiles Qt6::Core
    PRIVATE saveforge::zstd
)
saveforge_set_warnings(saveforge_vault)
