find_package(Qt6 REQUIRED COMPONENTS Test)

# saveforge_add_test(<nome> SOURCES ... LIBS ...)
function(saveforge_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    qt_add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LIBS})
    saveforge_set_warnings(${name})
    add_test(NAME ${name} COMMAND ${name})
    # Testes de UI rodam sem display.
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

saveforge_add_test(tst_core_result
    SOURCES unit/tst_core_result.cpp
    LIBS saveforge::core)

saveforge_add_test(tst_ui_smoke
    SOURCES ui/tst_ui_smoke.cpp
    LIBS saveforge_uiplugin Qt6::Gui Qt6::Quick Qt6::QuickControls2)
