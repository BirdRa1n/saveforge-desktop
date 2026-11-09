qt_add_executable(saveforge_cli
    main.cpp
    CliSupport.h
    CliSupport.cpp
    Commands.h
    DetectCommand.cpp
    RegistryCommand.cpp
    VaultCommands.cpp
)
set_target_properties(saveforge_cli PROPERTIES OUTPUT_NAME saveforge-cli)
target_link_libraries(saveforge_cli PRIVATE saveforge::core saveforge::detection saveforge::profiles saveforge::registry saveforge::services Qt6::Core)
saveforge_set_warnings(saveforge_cli)

install(TARGETS saveforge_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
