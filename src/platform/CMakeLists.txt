qt_add_library(saveforge_platform STATIC
    include/saveforge/platform/Os.h
    include/saveforge/platform/AppPaths.h
    include/saveforge/platform/FileSync.h
    include/saveforge/platform/FolderMap.h
    include/saveforge/platform/Processes.h
    include/saveforge/platform/UserFolders.h
    Os.cpp
    AppPaths.cpp
    FileSync.cpp
    FolderMap.cpp
    Processes.cpp
    UserFolders.cpp
    WinePrefix.cpp
)
add_library(saveforge::platform ALIAS saveforge_platform)

target_include_directories(saveforge_platform PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(saveforge_platform PUBLIC saveforge::core Qt6::Core)
if(WIN32)
    # SHGetKnownFolderPath / CoTaskMemFree
    target_link_libraries(saveforge_platform PRIVATE shell32 ole32)
endif()
saveforge_set_warnings(saveforge_platform)
