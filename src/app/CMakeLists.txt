# --- Módulo QML da interface (biblioteca estática, reutilizada pelos testes) ---
qt_add_library(savevault_ui STATIC)

set_source_files_properties(qml/theme/Theme.qml qml/theme/Icons.qml qml/theme/Format.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(savevault_ui
    URI SaveVault.Ui
    VERSION 1.0
    DEPENDENCIES SaveVault.Backend
    QML_FILES
        qml/Main.qml
        qml/theme/Theme.qml
        qml/theme/Icons.qml
        qml/theme/Format.qml
        qml/components/Icon.qml
        qml/components/Logo.qml
        qml/components/NavButton.qml
        qml/components/PrimaryButton.qml
        qml/components/SecondaryButton.qml
        qml/components/FilterChip.qml
        qml/components/SearchInput.qml
        qml/components/StatusBadge.qml
        qml/components/GameRow.qml
        qml/components/TitleBar.qml
        qml/components/HintBar.qml
        qml/components/KeyCap.qml
        qml/components/SectionLabel.qml
        qml/components/LinkButton.qml
        qml/components/EmptyState.qml
        qml/components/Sidebar.qml
        qml/components/Spinner.qml
        qml/components/Toast.qml
        qml/components/ToggleSetting.qml
        qml/components/FormField.qml
        qml/components/RoundedImage.qml
        qml/components/ActionMenu.qml
        qml/components/CheckItem.qml
        qml/components/IconButton.qml
        qml/pages/ActivityPage.qml
        qml/pages/AddGameDialog.qml
        qml/pages/GameDetailPage.qml
        qml/pages/LibraryPage.qml
        qml/pages/PlaceholderPage.qml
        qml/pages/RestoreDialog.qml
        qml/pages/SaveSlotDialog.qml
        qml/pages/SettingsPage.qml
        qml/pages/TransferPage.qml
    RESOURCES
        fonts/inter/Inter-Regular.ttf
        fonts/inter/Inter-Medium.ttf
        fonts/inter/Inter-SemiBold.ttf
        fonts/inter/Inter-Bold.ttf
        fonts/outfit/Outfit-Regular.ttf
        fonts/outfit/Outfit-Medium.ttf
        fonts/outfit/Outfit-SemiBold.ttf
        fonts/outfit/Outfit-Bold.ttf
        fonts/jetbrains-mono/JetBrainsMono-Regular.ttf
        fonts/jetbrains-mono/JetBrainsMono-Medium.ttf
)
target_link_libraries(savevault_ui PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::QuickDialogs2 savevault_backend)

# --- Executável GUI ---
qt_add_executable(savevault_app WIN32 MACOSX_BUNDLE main.cpp TrayIcon.h TrayIcon.cpp)

# App icon (source: icons/savevault.svg; regenerate with tools/icons/generate.py).
# The PNG is the window icon at run time; the .rc and .icns give the
# executable and the macOS bundle their icon in the OS shell.
qt_add_resources(savevault_app savevault_app_icon
    PREFIX "/savevault"
    FILES icons/savevault.png
)
if(WIN32)
    target_sources(savevault_app PRIVATE savevault.rc)
elseif(APPLE)
    set_source_files_properties(icons/savevault.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(savevault_app PRIVATE icons/savevault.icns)
endif()

set_target_properties(savevault_app PROPERTIES
    OUTPUT_NAME SaveVault
    MACOSX_BUNDLE_ICON_FILE savevault.icns
    MACOSX_BUNDLE_BUNDLE_NAME SaveVault
    MACOSX_BUNDLE_GUI_IDENTIFIER org.savevault.desktop
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)
target_link_libraries(savevault_app PRIVATE
    savevault::core
    savevault_uiplugin
    savevault_backendplugin
    Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Widgets
    savevault_backend
)
savevault_set_warnings(savevault_app)

install(TARGETS savevault_app
    BUNDLE DESTINATION . COMPONENT SaveVault
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT SaveVault
)

# Desktop entry and icon, so Linux desktops can list the app once the files
# are in a share/ directory they read (e.g. ~/.local after unpacking there).
if(UNIX AND NOT APPLE)
    install(FILES savevault.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications COMPONENT SaveVault)
    install(FILES icons/savevault.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps COMPONENT SaveVault)
endif()

# Copies the Qt libraries, plugins and QML modules the app uses next to it
# (windeployqt/macdeployqt under the hood), so `cmake --install` and the
# packages in cmake/SaveVaultPackaging.cmake run without a Qt installation.
qt_generate_deploy_qml_app_script(
    TARGET savevault_app
    OUTPUT_SCRIPT savevault_deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${savevault_deploy_script} COMPONENT SaveVault)
