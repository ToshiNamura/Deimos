set(DEIMOS_CLIENT_SOURCES main.cpp)

if(DEIMOS_BUILD_CLI)
    list(APPEND DEIMOS_CLIENT_SOURCES cli.cpp)
endif()

set(DEIMOS_CLIENT_UI_SOURCES)
if(DEIMOS_BUILD_UI)
    set(DEIMOS_CLIENT_UI_SOURCES
        ui/app.cpp
        ui/theme.cpp
        ui/widgets.cpp
        ui/window.cpp
        ui/views/dashboard.cpp
        ui/views/daemons.cpp
        ui/views/logs.cpp
        ui/views/profiles.cpp)
endif()

if(DEIMOS_BUILD_UI)
    # One console binary for both roles: console subsystem keeps scripts, pipes and
    # exit codes working; the GUI path detaches the console before opening a window.
    add_executable(deimos ${DEIMOS_CLIENT_SOURCES} ${DEIMOS_CLIENT_UI_SOURCES})
else()
    add_executable(deimos ${DEIMOS_CLIENT_SOURCES})
endif()

target_include_directories(deimos PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/ui")

if(DEIMOS_BUILD_CLI)
    target_compile_definitions(deimos PRIVATE DEIMOS_HAS_CLI=1)
endif()
if(DEIMOS_BUILD_UI)
    target_compile_definitions(deimos PRIVATE DEIMOS_HAS_UI=1)
    target_link_libraries(deimos PRIVATE Deimos::imgui glfw)
    if(WIN32)
        target_link_libraries(deimos PRIVATE opengl32)
    endif()
endif()

target_link_libraries(deimos PRIVATE Deimos::core)
deimos_configure_target(deimos)

if(DEIMOS_BUILD_CLI)
    install(TARGETS deimos
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT cli)
else()
    install(TARGETS deimos
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT ui)
endif()
