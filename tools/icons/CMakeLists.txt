# Stand-alone helper project; see README.md. Not added to the main build.
cmake_minimum_required(VERSION 3.24)
project(SaveForgeIconRenderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
find_package(Qt6 REQUIRED COMPONENTS Gui Svg)
qt_add_executable(render render.cpp)
target_link_libraries(render PRIVATE Qt6::Gui Qt6::Svg)
