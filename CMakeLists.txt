cmake_minimum_required(VERSION 3.20)

project(SIXERC
    VERSION 1.1.0
    DESCRIPTION "SIXERC native Windows dashboard"
    LANGUAGES CXX
)

if(NOT WIN32)
    message(FATAL_ERROR "SIXERC currently targets Windows.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(SIXERC WIN32
    src/main.cpp
)

target_compile_definitions(SIXERC PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    SIXERC_VERSION="${PROJECT_VERSION}"
    SIXERC_AUTHOR="Anonymous-beta (Chinedu)"
    SIXERC_CODENAME="Ekwensu's Grasp"
)

target_link_libraries(SIXERC PRIVATE
    comdlg32
    shell32
    user32
    gdi32
)

if(MSVC)
    target_compile_options(SIXERC PRIVATE
        /W4
        /permissive-
        /EHsc
        /utf-8
        /sdl
        /GS
        /guard:cf
    )

    target_link_options(SIXERC PRIVATE
        /DYNAMICBASE
        /NXCOMPAT
        /guard:cf
    )
endif()

set_target_properties(SIXERC PROPERTIES
    OUTPUT_NAME "SIXERC"
    WIN32_EXECUTABLE TRUE
)

install(
    TARGETS SIXERC
    RUNTIME DESTINATION .
)