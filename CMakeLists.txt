cmake_minimum_required(VERSION 3.25)

project(Deimos
    VERSION 0.1.0
    DESCRIPTION "Cross-platform DPI bypass control plane for zapret"
    HOMEPAGE_URL "https://github.com/ToshiNamura/Deimos"
    LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

include(DeimosOptions)
include(DeimosCompilerOptions)
include(DeimosDependencies)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/deimos/version.hpp"
    @ONLY)

add_subdirectory(src/core)

if(DEIMOS_BUILD_DAEMON)
    add_subdirectory(src/daemon)
endif()

if(DEIMOS_BUILD_CLI OR DEIMOS_BUILD_UI)
    add_subdirectory(src/client)
endif()

if(DEIMOS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(DeimosPackaging)
include(DeimosSummary)
