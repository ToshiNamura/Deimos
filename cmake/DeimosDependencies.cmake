include_guard(GLOBAL)

include(FetchContent)

# Pinned third-party versions. Bump deliberately, never implicitly.
set(DEIMOS_JSON_VERSION 3.11.3)
set(DEIMOS_HTTPLIB_VERSION 0.66.0)
set(DEIMOS_CATCH2_VERSION 3.16.1)
set(DEIMOS_GLFW_VERSION 3.4)
set(DEIMOS_IMGUI_VERSION 1.92.9b)

function(deimos_fetch_content name url)
    if(NOT DEIMOS_FETCH_DEPS)
        message(FATAL_ERROR
            "Dependency '${name}' is not installed and DEIMOS_FETCH_DEPS=OFF.\n"
            "Install it system-wide or reconfigure with -DDEIMOS_FETCH_DEPS=ON.")
    endif()
    FetchContent_Declare(${name} URL ${url} DOWNLOAD_EXTRACT_TIMESTAMP OFF)
endfunction()

# ---------------------------------------------------------------- nlohmann/json
find_package(nlohmann_json 3.11 QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "Deimos: fetching nlohmann/json ${DEIMOS_JSON_VERSION}")
    deimos_fetch_content(nlohmann_json
        "https://github.com/nlohmann/json/releases/download/v${DEIMOS_JSON_VERSION}/json.tar.xz")
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# ------------------------------------------------------------------ cpp-httplib
find_package(httplib ${DEIMOS_HTTPLIB_VERSION} QUIET)
if(NOT httplib_FOUND)
    message(STATUS "Deimos: fetching cpp-httplib ${DEIMOS_HTTPLIB_VERSION}")
    set(HTTPLIB_COMPILE OFF CACHE BOOL "" FORCE)
    set(HTTPLIB_INSTALL OFF CACHE BOOL "" FORCE)
    set(HTTPLIB_TEST OFF CACHE BOOL "" FORCE)
    set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
    set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
    set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
    if(DEIMOS_ENABLE_TLS)
        set(HTTPLIB_REQUIRE_OPENSSL ON CACHE BOOL "" FORCE)
    else()
        set(HTTPLIB_REQUIRE_OPENSSL OFF CACHE BOOL "" FORCE)
        set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
    endif()
    deimos_fetch_content(httplib
        "https://github.com/yhirose/cpp-httplib/archive/refs/tags/v${DEIMOS_HTTPLIB_VERSION}.tar.gz")
    FetchContent_MakeAvailable(httplib)
endif()

# ---------------------------------------------------------------------- Catch2
if(DEIMOS_BUILD_TESTS)
    find_package(Catch2 3 QUIET)
    if(NOT Catch2_FOUND)
        message(STATUS "Deimos: fetching Catch2 ${DEIMOS_CATCH2_VERSION}")
        set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
        set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
        deimos_fetch_content(catch2
            "https://github.com/catchorg/Catch2/archive/refs/tags/v${DEIMOS_CATCH2_VERSION}.tar.gz")
        FetchContent_MakeAvailable(catch2)
        list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    endif()
endif()

# ------------------------------------------------------------------ GLFW + ImGui
if(DEIMOS_BUILD_UI)
    find_package(glfw3 ${DEIMOS_GLFW_VERSION} QUIET)
    if(NOT glfw3_FOUND)
        message(STATUS "Deimos: fetching GLFW ${DEIMOS_GLFW_VERSION}")
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
        deimos_fetch_content(glfw
            "https://github.com/glfw/glfw/archive/refs/tags/${DEIMOS_GLFW_VERSION}.tar.gz")
        FetchContent_MakeAvailable(glfw)
    endif()

    # ImGui has no versioned package API; accept whatever system copy exists.
    find_package(imgui QUIET)
    if(NOT imgui_FOUND)
        message(STATUS "Deimos: fetching Dear ImGui ${DEIMOS_IMGUI_VERSION}")
        deimos_fetch_content(imgui
            "https://github.com/ocornut/imgui/archive/refs/tags/v${DEIMOS_IMGUI_VERSION}.tar.gz")
        FetchContent_MakeAvailable(imgui)

        find_package(OpenGL QUIET)
        add_library(deimos_imgui STATIC
            "${imgui_SOURCE_DIR}/imgui.cpp"
            "${imgui_SOURCE_DIR}/imgui_draw.cpp"
            "${imgui_SOURCE_DIR}/imgui_tables.cpp"
            "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
            "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
            "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp")
        target_include_directories(deimos_imgui SYSTEM PUBLIC
            "${imgui_SOURCE_DIR}"
            "${imgui_SOURCE_DIR}/backends")
        target_link_libraries(deimos_imgui PUBLIC glfw)
        if(TARGET OpenGL::GL)
            target_link_libraries(deimos_imgui PUBLIC OpenGL::GL)
        endif()
        add_library(Deimos::imgui ALIAS deimos_imgui)
    endif()
endif()
