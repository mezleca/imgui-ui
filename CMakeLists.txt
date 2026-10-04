cmake_minimum_required(VERSION 3.25)
project(imgui-ui LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(IMGUI_UI_BUILD_SDL "build the SDL3/OpenGL backend" OFF)
option(IMGUI_UI_BUILD_RAYLIB "build the raylib backend" OFF)
option(IMGUI_UI_BUILD_TESTS "build the framework tests" ${PROJECT_IS_TOP_LEVEL})
option(IMGUI_UI_BUILD_EXAMPLES "build the selected backend examples" OFF)
option(IMGUI_UI_EXTERNAL_IMGUI "build imgui from user-provided sources" OFF)
option(IMGUI_UI_ENABLE_SANITIZERS "enable sanitizers in debug builds" OFF)

set(IMGUI_UI_IMGUI_DIR "" CACHE PATH "external imgui source directory")

include(cmake/sanitizers.cmake)
include(cmake/dependencies.cmake)
add_subdirectory(src)

if(IMGUI_UI_BUILD_TESTS)
    add_subdirectory(vendor/Catch2 EXCLUDE_FROM_ALL)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vendor/Catch2/extras")
    include(CTest)
    add_subdirectory(tests)
endif()

if(IMGUI_UI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
