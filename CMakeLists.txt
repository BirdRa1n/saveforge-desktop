cmake_minimum_required(VERSION 3.24)

project(SaveVault
    VERSION 0.3.0
    DESCRIPTION "Gerenciador open source de backups de saves de jogos de PC"
    HOMEPAGE_URL "https://github.com/BirdRa1n/savevault-desktop"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SAVEVAULT_BUILD_TESTS "Compilar os testes" ON)
option(SAVEVAULT_WARNINGS_AS_ERRORS "Tratar avisos como erros" OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2 QuickDialogs2 Concurrent Widgets)
qt_standard_project_setup(REQUIRES 6.8)
# Módulos QML vão para <build>/qml/<URI>, como o qmllint e o qmlls esperam.
set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(SaveVaultWarnings)
include(GNUInstallDirs)
include(SaveVaultDependencies)

# Shared specifications (schemas, placeholder catalog, path rules, test vectors).
set(SAVEVAULT_SPECS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/specs" CACHE PATH "Path to savevault-specs")
if(NOT EXISTS "${SAVEVAULT_SPECS_DIR}/profile/placeholders.json")
    message(FATAL_ERROR
        "savevault-specs not found at ${SAVEVAULT_SPECS_DIR}.\n"
        "Run: git submodule update --init")
endif()

add_subdirectory(src)
include(SaveVaultPackaging)

if(SAVEVAULT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
