qt_add_executable(saveforge_cli
    main.cpp
    CliSupport.h
    CliSupport.cpp
    Commands.h
    DetectCommand.cpp
    ProfilesCommand.cpp
    TransferCommands.cpp
    RegistryCommand.cpp
    VaultCommands.cpp
)
set_target_properties(saveforge_cli PROPERTIES OUTPUT_NAME saveforge-cli)
target_link_libraries(saveforge_cli PRIVATE saveforge::core saveforge::detection saveforge::profiles saveforge::registry saveforge::services saveforge::archive Qt6::Core)
saveforge_set_warnings(saveforge_cli)

# Shipped next to the app on Windows and Linux, where it shares the Qt
# libraries the app's deploy step installs. The macOS disk image holds only
# SaveForge.app.
if(NOT APPLE)
    set_target_properties(saveforge_cli PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
    install(TARGETS saveforge_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT SaveForge)
endif()
