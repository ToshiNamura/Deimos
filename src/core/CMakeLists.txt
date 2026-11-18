add_library(deimos_core STATIC
    support/error.cpp
    support/log.cpp
    support/platform.cpp
    support/strings.cpp

    model/config.cpp
    model/desync.cpp
    model/diagnostic.cpp
    model/profile.cpp

    zapret/command_builder.cpp
    zapret/toolchain.cpp

    transport/daemon_client.cpp
    transport/http_server.cpp
    transport/protocol.cpp

    system/process.cpp)

add_library(Deimos::core ALIAS deimos_core)

target_include_directories(deimos_core
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_BINARY_DIR}/generated"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(deimos_core
    PUBLIC
        nlohmann_json::nlohmann_json
    PRIVATE
        httplib::httplib)

deimos_configure_target(deimos_core)
