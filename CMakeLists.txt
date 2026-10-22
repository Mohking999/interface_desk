cmake_minimum_required(VERSION 3.16)
project(DeskPilot VERSION 1.0.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "DeskPilot uses the Win32 API and must be built on Windows.")
endif()

enable_language(RC)

set(DESKPILOT_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${DESKPILOT_GENERATED_DIR}")

configure_file(
    deskpilot_version.h.in
    "${DESKPILOT_GENERATED_DIR}/deskpilot_version.h"
    @ONLY
)
configure_file(
    deskpilot.rc.in
    "${DESKPILOT_GENERATED_DIR}/deskpilot_version.rc"
    @ONLY
)
configure_file(
    installer.iss.in
    "${CMAKE_CURRENT_BINARY_DIR}/installer.iss"
    @ONLY
)

add_executable(DeskPilot WIN32
    command_palette_win32.cpp
    "${DESKPILOT_GENERATED_DIR}/deskpilot_version.rc"
)
target_compile_features(DeskPilot PRIVATE cxx_std_17)
target_compile_definitions(DeskPilot PRIVATE UNICODE _UNICODE)
target_include_directories(DeskPilot PRIVATE "${DESKPILOT_GENERATED_DIR}")
target_link_libraries(DeskPilot PRIVATE user32 gdi32 shell32 ole32 comdlg32)

if(MSVC)
    target_compile_options(DeskPilot PRIVATE /utf-8)
endif()
