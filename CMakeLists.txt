cmake_minimum_required(VERSION 3.24)

project(SaveForge
    VERSION 0.1.0
    DESCRIPTION "Gerenciador open source de backups de saves de jogos de PC"
    HOMEPAGE_URL "https://github.com/BirdRa1n/saveforge-desktop"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SAVEFORGE_BUILD_TESTS "Compilar os testes" ON)
option(SAVEFORGE_WARNINGS_AS_ERRORS "Tratar avisos como erros" OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2 QuickDialogs2 Concurrent)
qt_standard_project_setup(REQUIRES 6.8)
# Módulos QML vão para <build>/qml/<URI>, como o qmllint e o qmlls esperam.
set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(SaveForgeWarnings)
include(GNUInstallDirs)
include(SaveForgeDependencies)

# Shared specifications (schemas, placeholder catalog, path rules, test vectors).
set(SAVEFORGE_SPECS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/specs" CACHE PATH "Path to saveforge-specs")
if(NOT EXISTS "${SAVEFORGE_SPECS_DIR}/profile/placeholders.json")
    message(FATAL_ERROR
        "saveforge-specs not found at ${SAVEFORGE_SPECS_DIR}.\n"
        "Run: git submodule update --init")
endif()

add_subdirectory(src)
include(SaveForgePackaging)

if(SAVEFORGE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
