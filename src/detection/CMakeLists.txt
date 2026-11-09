qt_add_library(saveforge_detection STATIC
    include/saveforge/detection/DetectionEnvironment.h
    include/saveforge/detection/GameMatcher.h
    include/saveforge/detection/InstallCandidate.h
    include/saveforge/detection/Launchers.h
    include/saveforge/detection/Vdf.h
    DetectionEnvironment.cpp
    EpicGogDetector.cpp
    GameMatcher.cpp
    HeroicDetector.cpp
    InstallCandidate.cpp
    SteamDetector.cpp
    Vdf.cpp
)
add_library(saveforge::detection ALIAS saveforge_detection)

target_include_directories(saveforge_detection PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(saveforge_detection PUBLIC saveforge::core saveforge::platform saveforge::profiles Qt6::Core)
saveforge_set_warnings(saveforge_detection)
