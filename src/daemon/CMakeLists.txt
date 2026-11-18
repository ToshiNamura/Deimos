add_library(deimos_daemon STATIC
    api_server.cpp
    config_store.cpp
    daemon_app.cpp
    engine.cpp
    service.cpp)

target_include_directories(deimos_daemon
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(deimos_daemon
    PUBLIC
        Deimos::core)

if(WIN32)
    target_link_libraries(deimos_daemon PUBLIC advapi32)
endif()

deimos_configure_target(deimos_daemon)

add_executable(deimosd main.cpp)
target_link_libraries(deimosd PRIVATE deimos_daemon)
deimos_configure_target(deimosd)

install(TARGETS deimosd
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT daemon)
