# QML module SaveVault.Backend: the C++ side of the interface (controller and
# list models). It holds no layout or styling; SaveVault.Ui renders it.
qt_add_library(savevault_backend STATIC)

qt_add_qml_module(savevault_backend
    URI SaveVault.Backend
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
target_link_libraries(savevault_backend PUBLIC savevault::services Qt6::Qml Qt6::Gui Qt6::Concurrent)
# The executable talks to AppController directly (tray icon), so its headers are public.
target_include_directories(savevault_backend PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
savevault_set_warnings(savevault_backend)
