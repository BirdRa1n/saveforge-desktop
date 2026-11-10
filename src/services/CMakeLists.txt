qt_add_library(savevault_services STATIC
    include/savevault/services/AutoBackup.h
    include/savevault/services/BackupService.h
    include/savevault/services/GameArt.h
    include/savevault/services/GameTarget.h
    include/savevault/services/Library.h
    include/savevault/services/LocalProfiles.h
    include/savevault/services/RestoreService.h
    include/savevault/services/Retention.h
    include/savevault/services/SaveSlots.h
    include/savevault/services/Transfer.h
    AutoBackup.cpp
    BackupService.cpp
    GameArt.cpp
    GameTarget.cpp
    Library.cpp
    LocalProfiles.cpp
    RestoreService.cpp
    Retention.cpp
    SaveSlots.cpp
    Transfer.cpp
)
add_library(savevault::services ALIAS savevault_services)

target_include_directories(savevault_services PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(savevault_services
    PUBLIC savevault::core savevault::platform savevault::profiles savevault::vault savevault::archive savevault::detection savevault::registry Qt6::Core
)
savevault_set_warnings(savevault_services)
