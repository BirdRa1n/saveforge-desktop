qt_add_library(savevault_platform STATIC
    include/savevault/platform/Os.h
    include/savevault/platform/AppPaths.h
    include/savevault/platform/FileSync.h
    include/savevault/platform/FolderMap.h
    include/savevault/platform/Processes.h
    include/savevault/platform/RegistryStore.h
    include/savevault/platform/UserFolders.h
    Os.cpp
    AppPaths.cpp
    FileSync.cpp
    FolderMap.cpp
    Processes.cpp
    RegistryStore.cpp
    UserFolders.cpp
    WinePrefix.cpp
)
add_library(savevault::platform ALIAS savevault_platform)

target_include_directories(savevault_platform PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(savevault_platform PUBLIC savevault::core Qt6::Core)
if(WIN32)
    # SHGetKnownFolderPath / CoTaskMemFree; registry functions (advapi32)
    target_link_libraries(savevault_platform PRIVATE shell32 ole32 advapi32)
endif()
savevault_set_warnings(savevault_platform)
