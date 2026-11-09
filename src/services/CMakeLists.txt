qt_add_library(saveforge_services STATIC
    include/saveforge/services/BackupService.h
    include/saveforge/services/GameTarget.h
    include/saveforge/services/Library.h
    include/saveforge/services/LocalProfiles.h
    include/saveforge/services/RestoreService.h
    BackupService.cpp
    GameTarget.cpp
    Library.cpp
    LocalProfiles.cpp
    RestoreService.cpp
)
add_library(saveforge::services ALIAS saveforge_services)

target_include_directories(saveforge_services PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(saveforge_services
    PUBLIC saveforge::core saveforge::platform saveforge::profiles saveforge::vault saveforge::detection saveforge::registry Qt6::Core
)
saveforge_set_warnings(saveforge_services)
