# --- Módulo QML da interface (biblioteca estática, reutilizada pelos testes) ---
qt_add_library(saveforge_ui STATIC)

set_source_files_properties(qml/theme/Theme.qml qml/theme/Icons.qml qml/theme/Format.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(saveforge_ui
    URI SaveForge.Ui
    VERSION 1.0
    DEPENDENCIES SaveForge.Backend
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
        qml/components/GameCard.qml
        qml/components/EmptyState.qml
        qml/components/Sidebar.qml
        qml/components/Spinner.qml
        qml/components/Toast.qml
        qml/components/ToggleSetting.qml
        qml/components/FormField.qml
        qml/pages/ActivityPage.qml
        qml/pages/AddGameDialog.qml
        qml/pages/GameDetailPage.qml
        qml/pages/LibraryPage.qml
        qml/pages/PlaceholderPage.qml
        qml/pages/RestoreDialog.qml
        qml/pages/SettingsPage.qml
)
target_link_libraries(saveforge_ui PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::QuickDialogs2 saveforge_backend)

# --- Executável GUI ---
qt_add_executable(saveforge_app WIN32 MACOSX_BUNDLE main.cpp)
set_target_properties(saveforge_app PROPERTIES
    OUTPUT_NAME SaveForge
    MACOSX_BUNDLE_GUI_IDENTIFIER org.saveforge.desktop
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)
target_link_libraries(saveforge_app PRIVATE
    saveforge::core
    saveforge_uiplugin
    saveforge_backendplugin
    Qt6::Gui Qt6::Quick Qt6::QuickControls2
)
saveforge_set_warnings(saveforge_app)

install(TARGETS saveforge_app
    BUNDLE DESTINATION . COMPONENT SaveForge
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT SaveForge
)

# Copies the Qt libraries, plugins and QML modules the app uses next to it
# (windeployqt/macdeployqt under the hood), so `cmake --install` and the
# packages in cmake/SaveForgePackaging.cmake run without a Qt installation.
qt_generate_deploy_qml_app_script(
    TARGET saveforge_app
    OUTPUT_SCRIPT saveforge_deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${saveforge_deploy_script} COMPONENT SaveForge)
