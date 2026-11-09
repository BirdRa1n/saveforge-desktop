find_package(Qt6 REQUIRED COMPONENTS Test)

# saveforge_add_test(<nome> SOURCES ... LIBS ...)
function(saveforge_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    qt_add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LIBS})
    # Conformance tests read vectors and examples straight from the specs submodule.
    target_compile_definitions(${name} PRIVATE SAVEFORGE_SPECS_DIR="${SAVEFORGE_SPECS_DIR}")
    saveforge_set_warnings(${name})
    add_test(NAME ${name} COMMAND ${name})
    # Testes de UI rodam sem display.
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

saveforge_add_test(tst_core_result
    SOURCES unit/tst_core_result.cpp
    LIBS saveforge::core)

saveforge_add_test(tst_platform
    SOURCES unit/tst_platform.cpp
    LIBS saveforge::platform)

saveforge_add_test(tst_path_rules
    SOURCES unit/tst_path_rules.cpp
    LIBS saveforge::profiles)

saveforge_add_test(tst_profile_parser
    SOURCES unit/tst_profile_parser.cpp
    LIBS saveforge::profiles)

saveforge_add_test(tst_path_resolver
    SOURCES unit/tst_path_resolver.cpp
    LIBS saveforge::profiles)

saveforge_add_test(tst_snapshot_json
    SOURCES unit/tst_snapshot_json.cpp
    LIBS saveforge::vault)

saveforge_add_test(tst_blob_store
    SOURCES unit/tst_blob_store.cpp
    LIBS saveforge::vault)

saveforge_add_test(tst_vault
    SOURCES unit/tst_vault.cpp
    LIBS saveforge::vault)

saveforge_add_test(tst_vdf
    SOURCES unit/tst_vdf.cpp
    LIBS saveforge::detection)

saveforge_add_test(tst_ui_smoke
    SOURCES ui/tst_ui_smoke.cpp
    LIBS saveforge_uiplugin Qt6::Gui Qt6::Quick Qt6::QuickControls2)
