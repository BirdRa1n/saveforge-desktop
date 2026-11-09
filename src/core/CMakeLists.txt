configure_file(Version.h.in "${CMAKE_CURRENT_BINARY_DIR}/include/saveforge/core/Version.h" @ONLY)

qt_add_library(saveforge_core STATIC
    include/saveforge/core/Error.h
    include/saveforge/core/Result.h
    Error.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/include/saveforge/core/Version.h"
)
add_library(saveforge::core ALIAS saveforge_core)

target_include_directories(saveforge_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/include"
)
# O núcleo depende apenas de QtCore (sem GUI), para ser usado pela CLI e pelos testes.
target_link_libraries(saveforge_core PUBLIC Qt6::Core)
saveforge_set_warnings(saveforge_core)
