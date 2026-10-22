cmake_minimum_required(VERSION 3.16)
project(CommandPalette LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 COMPONENTS Widgets REQUIRED)

add_executable(command_palette command_palette.cpp)
target_link_libraries(command_palette PRIVATE Qt6::Widgets)
