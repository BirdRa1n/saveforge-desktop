qt_add_executable(savevault_cli
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
set_target_properties(savevault_cli PROPERTIES OUTPUT_NAME savevault-cli)
target_link_libraries(savevault_cli PRIVATE savevault::core savevault::detection savevault::profiles savevault::registry savevault::services savevault::archive Qt6::Core)
savevault_set_warnings(savevault_cli)

# Shipped next to the app on Windows and Linux, where it shares the Qt
# libraries the app's deploy step installs. The macOS disk image holds only
# SaveVault.app.
if(NOT APPLE)
    set_target_properties(savevault_cli PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
    install(TARGETS savevault_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT SaveVault)
endif()
