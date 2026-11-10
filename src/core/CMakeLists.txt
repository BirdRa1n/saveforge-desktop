configure_file(Version.h.in "${CMAKE_CURRENT_BINARY_DIR}/include/savevault/core/Version.h" @ONLY)

qt_add_library(savevault_core STATIC
    include/savevault/core/Error.h
    include/savevault/core/JsonReader.h
    include/savevault/core/Result.h
    include/savevault/core/Uuid.h
    Error.cpp
    JsonReader.cpp
    Uuid.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/include/savevault/core/Version.h"
)
add_library(savevault::core ALIAS savevault_core)

target_include_directories(savevault_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/include"
)
# O núcleo depende apenas de QtCore (sem GUI), para ser usado pela CLI e pelos testes.
target_link_libraries(savevault_core PUBLIC Qt6::Core)
savevault_set_warnings(savevault_core)
