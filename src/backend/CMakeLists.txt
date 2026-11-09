# QML module SaveForge.Backend: the C++ side of the interface (controller and
# list models). It holds no layout or styling; SaveForge.Ui renders it.
qt_add_library(saveforge_backend STATIC)

qt_add_qml_module(saveforge_backend
    URI SaveForge.Backend
    VERSION 1.0
    SOURCES
        AppController.h
        AppController.cpp
        AppSettings.h
        AppSettings.cpp
        Formatting.h
        Formatting.cpp
        LibraryModel.h
        LibraryModel.cpp
)
target_link_libraries(saveforge_backend PUBLIC saveforge::services Qt6::Qml Qt6::Gui Qt6::Concurrent)
saveforge_set_warnings(saveforge_backend)
