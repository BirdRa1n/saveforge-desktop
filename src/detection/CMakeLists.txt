qt_add_library(savevault_detection STATIC
    include/savevault/detection/DetectionEnvironment.h
    include/savevault/detection/GameMatcher.h
    include/savevault/detection/InstallCandidate.h
    include/savevault/detection/Launchers.h
    include/savevault/detection/Vdf.h
    DetectionEnvironment.cpp
    EpicGogDetector.cpp
    GameMatcher.cpp
    HeroicDetector.cpp
    LutrisDetector.cpp
    InstallCandidate.cpp
    SteamDetector.cpp
    Vdf.cpp
)
add_library(savevault::detection ALIAS savevault_detection)

target_include_directories(savevault_detection PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(savevault_detection PUBLIC savevault::core savevault::platform savevault::profiles Qt6::Core)
savevault_set_warnings(savevault_detection)
