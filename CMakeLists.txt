cmake_minimum_required(VERSION 3.25)
project(imgui-ui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(IMGUI_UI_BUILD_SDL "build the SDL3/OpenGL backend" OFF)
option(IMGUI_UI_BUILD_RAYLIB "build the raylib backend" OFF)
option(IMGUI_UI_BUILD_TESTS "build the framework tests" ${PROJECT_IS_TOP_LEVEL})
option(IMGUI_UI_BUILD_EXAMPLES "build the selected backend examples" OFF)

include(FetchContent)

if(NOT TARGET imgui)
    FetchContent_Declare(imgui_source
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG 1d7b37dc9b825a8aa49412e7f9a5546be0006ce4
    )
    FetchContent_MakeAvailable(imgui_source)

    add_library(imgui STATIC
        "${imgui_source_SOURCE_DIR}/imgui.cpp"
        "${imgui_source_SOURCE_DIR}/imgui_demo.cpp"
        "${imgui_source_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_source_SOURCE_DIR}/imgui_tables.cpp"
        "${imgui_source_SOURCE_DIR}/imgui_widgets.cpp"
        "${imgui_source_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp"
    )
    target_include_directories(imgui PUBLIC
        "${imgui_source_SOURCE_DIR}"
        "${imgui_source_SOURCE_DIR}/misc/cpp"
    )
    set(IMGUI_UI_IMGUI_BACKENDS_DIR "${imgui_source_SOURCE_DIR}/backends" CACHE INTERNAL "dear imgui backend sources")
endif()

if(NOT TARGET imgui-ui::imgui)
    add_library(imgui-ui::imgui ALIAS imgui)
endif()

if((IMGUI_UI_BUILD_SDL OR IMGUI_UI_BUILD_RAYLIB) AND NOT IMGUI_UI_IMGUI_BACKENDS_DIR)
    message(FATAL_ERROR "IMGUI_UI_IMGUI_BACKENDS_DIR is required by window backends")
endif()

if(IMGUI_UI_BUILD_SDL)
    find_package(OpenGL REQUIRED)
    find_package(SDL3 QUIET CONFIG)
    if(NOT TARGET SDL3::SDL3)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(sdl_source
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG d9d5536704d585616d4db3c8ba3c4ff6fc2757e1
        )
        FetchContent_MakeAvailable(sdl_source)
    endif()

    if(NOT TARGET lunasvg)
        set(LUNASVG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(lunasvg_source
            GIT_REPOSITORY https://github.com/sammycage/lunasvg.git
            GIT_TAG 2872affa1027cad92a05408a7e6f2547efa7f364
            GIT_SUBMODULES_RECURSE TRUE
        )
        FetchContent_MakeAvailable(lunasvg_source)
    endif()
endif()

if(IMGUI_UI_BUILD_RAYLIB AND NOT TARGET raylib)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(raylib_source
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 6.0
    )
    FetchContent_MakeAvailable(raylib_source)
endif()

add_subdirectory(ui)

if(IMGUI_UI_BUILD_TESTS)
    FetchContent_Declare(catch2_source
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG 69e0473f6e98d47c93518424c08ee69ee632c0f0
    )
    FetchContent_MakeAvailable(catch2_source)
    list(APPEND CMAKE_MODULE_PATH "${catch2_source_SOURCE_DIR}/extras")
    include(CTest)
    add_subdirectory(tests)
endif()

if(IMGUI_UI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
