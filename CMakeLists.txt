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

if(MSVC)
    foreach(config DEBUG RELWITHDEBINFO)
        string(REPLACE "/ZI" "/Zi" CMAKE_CXX_FLAGS_${config} "${CMAKE_CXX_FLAGS_${config}}")
        string(REGEX REPLACE "/RTC[1su]+" "" CMAKE_CXX_FLAGS_${config} "${CMAKE_CXX_FLAGS_${config}}")
    endforeach()
endif()

function(imgui_ui_enable_sanitizers target)
    if(MSVC)
        target_compile_options(${target} PUBLIC "$<$<CONFIG:Debug>:/fsanitize=address>")
        # keep msvc's stl annotation mode consistent with unsanitized dependencies.
        target_compile_definitions(${target} PUBLIC
            "$<$<CONFIG:Debug>:_DISABLE_STRING_ANNOTATION>"
            "$<$<CONFIG:Debug>:_DISABLE_VECTOR_ANNOTATION>"
            "$<$<CONFIG:Debug>:_DISABLE_OPTIONAL_ANNOTATION>"
        )
        target_link_options(${target} PUBLIC "$<$<CONFIG:Debug>:/INCREMENTAL:NO>")
    else()
        target_compile_options(${target} PUBLIC
            "$<$<CONFIG:Debug>:-fsanitize=address,undefined>"
            "$<$<CONFIG:Debug>:-fno-omit-frame-pointer>"
        )
        target_link_options(${target} PUBLIC "$<$<CONFIG:Debug>:-fsanitize=address,undefined>")
    endif()
endfunction()

if(NOT TARGET glad)
    add_subdirectory(vendor/glad EXCLUDE_FROM_ALL)
endif()
if(NOT TARGET imgui-ui::glad)
    add_library(imgui-ui::glad ALIAS glad)
endif()

if(NOT TARGET imgui)
    add_library(imgui STATIC
        vendor/imgui/imgui.cpp
        vendor/imgui/imgui_demo.cpp
        vendor/imgui/imgui_draw.cpp
        vendor/imgui/imgui_tables.cpp
        vendor/imgui/imgui_widgets.cpp
        vendor/imgui/misc/cpp/imgui_stdlib.cpp
    )

    target_include_directories(imgui PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui"
        "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/misc/cpp"
    )

    set(IMGUI_UI_IMGUI_BACKENDS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/backends" CACHE INTERNAL "dear imgui backend sources")
endif()

if(NOT TARGET imgui-ui::imgui)
    add_library(imgui-ui::imgui ALIAS imgui)
endif()

if((IMGUI_UI_BUILD_SDL OR IMGUI_UI_BUILD_RAYLIB) AND NOT IMGUI_UI_IMGUI_BACKENDS_DIR)
    message(FATAL_ERROR "IMGUI_UI_IMGUI_BACKENDS_DIR is required by window backends")
endif()

if(IMGUI_UI_BUILD_SDL)
    find_package(OpenGL REQUIRED)

    if(NOT TARGET SDL3::SDL3)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(SDL_TESTS OFF CACHE BOOL "" FORCE)
        add_subdirectory(vendor/SDL EXCLUDE_FROM_ALL)
    endif()

    if(NOT TARGET lunasvg)
        set(LUNASVG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        add_subdirectory(vendor/lunasvg EXCLUDE_FROM_ALL)
    endif()
endif()

if(IMGUI_UI_BUILD_RAYLIB AND NOT TARGET raylib)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(vendor/raylib EXCLUDE_FROM_ALL)
endif()

add_subdirectory(ui)

if(IMGUI_UI_BUILD_TESTS)
    add_subdirectory(vendor/Catch2 EXCLUDE_FROM_ALL)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vendor/Catch2/extras")
    include(CTest)
    add_subdirectory(tests)
endif()

if(IMGUI_UI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
