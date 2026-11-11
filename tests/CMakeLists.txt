find_package(Qt6 REQUIRED COMPONENTS Test)

# savevault_add_test(<nome> SOURCES ... LIBS ...)
function(savevault_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    qt_add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LIBS})
    target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/support")
    # Conformance tests read vectors and examples straight from the specs submodule.
    target_compile_definitions(${name} PRIVATE
        SAVEVAULT_SPECS_DIR="${SAVEVAULT_SPECS_DIR}"
        SAVEVAULT_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
    savevault_set_warnings(${name})
    add_test(NAME ${name} COMMAND ${name})
    # Testes de UI rodam sem display.
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

savevault_add_test(tst_core_result
    SOURCES unit/tst_core_result.cpp
    LIBS savevault::core)

savevault_add_test(tst_platform
    SOURCES unit/tst_platform.cpp
    LIBS savevault::platform)

savevault_add_test(tst_path_rules
    SOURCES unit/tst_path_rules.cpp
    LIBS savevault::profiles)

savevault_add_test(tst_profile_parser
    SOURCES unit/tst_profile_parser.cpp
    LIBS savevault::profiles)

savevault_add_test(tst_path_resolver
    SOURCES unit/tst_path_resolver.cpp
    LIBS savevault::profiles)

savevault_add_test(tst_snapshot_json
    SOURCES unit/tst_snapshot_json.cpp
    LIBS savevault::vault)

savevault_add_test(tst_blob_store
    SOURCES unit/tst_blob_store.cpp
    LIBS savevault::vault)

savevault_add_test(tst_vault
    SOURCES unit/tst_vault.cpp
    LIBS savevault::vault)

savevault_add_test(tst_vdf
    SOURCES unit/tst_vdf.cpp
    LIBS savevault::detection)

savevault_add_test(tst_launchers
    SOURCES unit/tst_launchers.cpp
    LIBS savevault::detection)

savevault_add_test(tst_game_matcher
    SOURCES unit/tst_game_matcher.cpp
    LIBS savevault::detection)

savevault_add_test(tst_backup_service
    SOURCES unit/tst_backup_service.cpp
    LIBS savevault::services)

savevault_add_test(tst_restore_service
    SOURCES unit/tst_restore_service.cpp
    LIBS savevault::services)

savevault_add_test(tst_gamesave
    SOURCES unit/tst_gamesave.cpp
    LIBS savevault::archive savevault::miniz)

savevault_add_test(tst_registry_saves
    SOURCES unit/tst_registry_saves.cpp
    LIBS savevault::services)

savevault_add_test(tst_retention
    SOURCES unit/tst_retention.cpp
    LIBS savevault::services)

savevault_add_test(tst_transfer
    SOURCES unit/tst_transfer.cpp
    LIBS savevault::services)

savevault_add_test(tst_auto_backup
    SOURCES unit/tst_auto_backup.cpp
    LIBS savevault::services)

savevault_add_test(tst_save_slots
    SOURCES unit/tst_save_slots.cpp
    LIBS savevault::services)

savevault_add_test(tst_game_art
    SOURCES unit/tst_game_art.cpp
    LIBS savevault::services)

savevault_add_test(tst_local_profiles
    SOURCES unit/tst_local_profiles.cpp
    LIBS savevault::services)

savevault_add_test(tst_ed25519
    SOURCES unit/tst_ed25519.cpp
    LIBS savevault::registry)

savevault_add_test(tst_json_spans
    SOURCES unit/tst_json_spans.cpp
    LIBS savevault::registry)

savevault_add_test(tst_registry_channel
    SOURCES unit/tst_registry_channel.cpp
    LIBS savevault::registry)

savevault_add_test(tst_registry_cache
    SOURCES unit/tst_registry_cache.cpp
    LIBS savevault::registry)

savevault_add_test(tst_registry_updater
    SOURCES unit/tst_registry_updater.cpp support/StaticHttpServer.h
    LIBS savevault::registry Qt6::Network)

savevault_add_test(tst_profile_catalog
    SOURCES unit/tst_profile_catalog.cpp
    LIBS savevault::registry)

savevault_add_test(tst_library
    SOURCES unit/tst_library.cpp
    LIBS savevault::services)

savevault_add_test(tst_ui_smoke
    SOURCES ui/tst_ui_smoke.cpp
    LIBS savevault_uiplugin savevault_backendplugin Qt6::Gui Qt6::Quick Qt6::QuickControls2)
