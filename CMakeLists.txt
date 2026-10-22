cmake_minimum_required(VERSION 3.16)
project(CommandPalette LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "CommandPalette uses the Win32 API and must be built on Windows.")
endif()

add_executable(command_palette WIN32 command_palette_win32.cpp)
target_compile_features(command_palette PRIVATE cxx_std_17)
target_compile_definitions(command_palette PRIVATE UNICODE _UNICODE)
target_link_libraries(command_palette PRIVATE user32 gdi32)

if(MSVC)
    target_compile_options(command_palette PRIVATE /utf-8)
endif()
