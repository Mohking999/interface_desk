cmake_minimum_required(VERSION 3.16)
project(DeskPilot LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "DeskPilot uses the Win32 API and must be built on Windows.")
endif()

enable_language(RC)

add_executable(DeskPilot WIN32 command_palette_win32.cpp deskpilot.rc)
target_compile_features(DeskPilot PRIVATE cxx_std_17)
target_compile_definitions(DeskPilot PRIVATE UNICODE _UNICODE)
target_include_directories(DeskPilot PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(DeskPilot PRIVATE user32 gdi32 shell32 ole32 comdlg32)

if(MSVC)
    target_compile_options(DeskPilot PRIVATE /utf-8)
endif()
