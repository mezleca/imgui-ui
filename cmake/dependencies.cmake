if(IMGUI_UI_EXTERNAL_IMGUI)
    get_filename_component(IMGUI_UI_IMGUI_DIR "${IMGUI_UI_IMGUI_DIR}" ABSOLUTE)
    if(NOT EXISTS "${IMGUI_UI_IMGUI_DIR}/imgui.h")
        message(FATAL_ERROR "IMGUI_UI_EXTERNAL_IMGUI requires IMGUI_UI_IMGUI_DIR to point to an imgui source directory")
    endif()
else()
    set(IMGUI_UI_IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui")
endif()

# core compiles the native file dialog adapter even when both render backends are disabled.
if(NOT TARGET nfd)
    set(NFD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(NFD_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory(vendor/nativefiledialog-extended EXCLUDE_FROM_ALL)
endif()

if(NOT TARGET imgui-ui::nfd)
    add_library(imgui-ui::nfd ALIAS nfd)
endif()

# imgui_freetype.cpp is always compiled below and requires the freetype headers and library.
if(NOT TARGET imgui-ui::imgui)
    if(NOT TARGET Freetype::Freetype)
        find_package(Freetype QUIET)
        if(NOT TARGET Freetype::Freetype)
            add_subdirectory(vendor/freetype EXCLUDE_FROM_ALL)
            add_library(Freetype::Freetype ALIAS freetype-interface)
        endif()
    endif()

    if(NOT TARGET imgui-ui-imgui)
        add_library(imgui-ui-imgui STATIC
            "${IMGUI_UI_IMGUI_DIR}/imgui.cpp"
            "${IMGUI_UI_IMGUI_DIR}/imgui_draw.cpp"
            "${IMGUI_UI_IMGUI_DIR}/imgui_tables.cpp"
            "${IMGUI_UI_IMGUI_DIR}/imgui_widgets.cpp"
            "${IMGUI_UI_IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp"
            "${IMGUI_UI_IMGUI_DIR}/misc/freetype/imgui_freetype.cpp"
        )

        target_include_directories(imgui-ui-imgui PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/include/imgui-ui/imgui"
            "${IMGUI_UI_IMGUI_DIR}"
            "${IMGUI_UI_IMGUI_DIR}/misc/cpp"
        )

        target_compile_definitions(imgui-ui-imgui PUBLIC IMGUI_ENABLE_FREETYPE PRIVATE IMGUI_DEFINE_MATH_OPERATORS)
        target_link_libraries(imgui-ui-imgui PUBLIC Freetype::Freetype)
        target_precompile_headers(imgui-ui-imgui PRIVATE
            "${IMGUI_UI_IMGUI_DIR}/imgui_internal.h"
        )
    endif()

    add_library(imgui-ui::imgui ALIAS imgui-ui-imgui)
endif()

# core does not compile the opengl effects or texture loaders below.
if(NOT IMGUI_UI_BUILD_SDL AND NOT IMGUI_UI_BUILD_RAYLIB)
    return()
endif()

if(NOT TARGET glad)
    add_subdirectory(vendor/glad EXCLUDE_FROM_ALL)
endif()

if(NOT TARGET imgui-ui::glad)
    add_library(imgui-ui::glad ALIAS glad)
endif()

find_package(OpenGL REQUIRED)

# libnsgif has no cmake target. compile its decoder from gif.c and lzw.c.
if(NOT TARGET nsgif)
    add_library(nsgif STATIC
        vendor/libnsgif/src/gif.c
        vendor/libnsgif/src/lzw.c
    )
    target_include_directories(nsgif PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/vendor/libnsgif/include")
endif()

if(NOT TARGET imgui-ui::nsgif)
    add_library(imgui-ui::nsgif ALIAS nsgif)
endif()

if(NOT TARGET lunasvg)
    set(LUNASVG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(vendor/lunasvg EXCLUDE_FROM_ALL)
endif()

if(NOT TARGET imgui-ui::lunasvg)
    add_library(imgui-ui::lunasvg ALIAS lunasvg)
endif()

# reuse sdl3 from the parent first, then try cmake, pkg-config and the vendored sources.
if(IMGUI_UI_BUILD_SDL AND NOT TARGET SDL3::SDL3)
    find_package(SDL3 QUIET CONFIG)

    if(NOT TARGET SDL3::SDL3)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL3_SYSTEM QUIET IMPORTED_TARGET sdl3)

            if(SDL3_SYSTEM_FOUND)
                add_library(SDL3::SDL3 ALIAS PkgConfig::SDL3_SYSTEM)
            endif()
        endif()
    endif()

    if(NOT TARGET SDL3::SDL3)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(SDL_TESTS OFF CACHE BOOL "" FORCE)
        add_subdirectory(vendor/SDL EXCLUDE_FROM_ALL)
    endif()
endif()

if(IMGUI_UI_BUILD_RAYLIB AND NOT TARGET raylib)
    if(UNIX AND NOT APPLE)
        set(GLFW_BUILD_WAYLAND ON CACHE BOOL "")
    endif()

    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(vendor/raylib EXCLUDE_FROM_ALL)
endif()
