include_guard(GLOBAL)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Third-party libraries must use the same CRT as Deimos, so the choice is global,
# not per target. Catch2 in particular fails to link when the runtimes differ.
if(MSVC AND DEIMOS_STATIC_RUNTIME)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

function(deimos_configure_target target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "deimos_configure_target: unknown target '${target}'")
    endif()

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus /EHsc)
        if(DEIMOS_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
        target_compile_definitions(${target} PRIVATE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
            _CRT_SECURE_NO_WARNINGS
            UNICODE
            _UNICODE)
        if(DEIMOS_STATIC_RUNTIME)
            set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
        if(DEIMOS_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()
