if(NOT TARGET deimos_daemon)
    message(STATUS "Deimos: tests require the daemon target, skipping tests")
    return()
endif()

add_executable(deimos_fake_zapret fake_zapret.cpp)
deimos_configure_target(deimos_fake_zapret)

add_executable(deimos_tests
    test_api.cpp
    test_command_builder.cpp
    test_config.cpp
    test_process.cpp
    test_protocol.cpp)

target_link_libraries(deimos_tests
    PRIVATE
        deimos_daemon
        Deimos::core
        Catch2::Catch2WithMain)

target_compile_definitions(deimos_tests
    PRIVATE
        DEIMOS_FAKE_ZAPRET="$<TARGET_FILE:deimos_fake_zapret>")

deimos_configure_target(deimos_tests)
add_dependencies(deimos_tests deimos_fake_zapret)

if(COMMAND catch_discover_tests)
    catch_discover_tests(deimos_tests
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
else()
    add_test(NAME deimos_tests COMMAND deimos_tests)
endif()
