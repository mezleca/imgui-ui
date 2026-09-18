function(imgui_ui_add_backend name)
    cmake_parse_arguments(ARG "" "" "SOURCES;PRIVATE_INCLUDE_DIRS;LINK_LIBS" ${ARGN})

    add_library(imgui-ui-${name} STATIC ${ARG_SOURCES})
    add_library(imgui-ui::${name} ALIAS imgui-ui-${name})
    imgui_ui_enable_sanitizers(imgui-ui-${name})
    target_precompile_headers(imgui-ui-${name} PRIVATE <imgui.h>)

    target_link_libraries(imgui-ui-${name} PUBLIC imgui-ui-core ${ARG_LINK_LIBS})

    if(ARG_PRIVATE_INCLUDE_DIRS)
        target_include_directories(imgui-ui-${name} PRIVATE ${ARG_PRIVATE_INCLUDE_DIRS})
    endif()
endfunction()

function(imgui_ui_link_plutovg target)
    if(TARGET plutovg)
        target_compile_definitions(${target} PRIVATE PLUTOVG_BUILD_STATIC)
        target_link_libraries(${target} PUBLIC plutovg::plutovg)
        message(STATUS "${target}: linked against plutovg")
    else()
        message(STATUS "${target}: plutovg not available, skipping")
    endif()
endfunction()

file(GLOB_RECURSE IMGUI_UI_CORE_SOURCES CONFIGURE_DEPENDS "*.cpp")
list(FILTER IMGUI_UI_CORE_SOURCES EXCLUDE REGEX "/backends/")

set(IMGUI_UI_OPENGL_EFFECT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/effects/opengl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/effects/blur/opengl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/effects/shadow/opengl.cpp"
)
list(REMOVE_ITEM IMGUI_UI_CORE_SOURCES ${IMGUI_UI_OPENGL_EFFECT_SOURCES})

add_library(imgui-ui-core STATIC ${IMGUI_UI_CORE_SOURCES})
add_library(imgui-ui::core ALIAS imgui-ui-core)
imgui_ui_enable_sanitizers(imgui-ui-core)
target_precompile_headers(imgui-ui-core PRIVATE <imgui.h>)

target_compile_features(imgui-ui-core PUBLIC cxx_std_20)
target_include_directories(imgui-ui-core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
target_link_libraries(imgui-ui-core PUBLIC imgui-ui::imgui)
target_link_libraries(imgui-ui-core PRIVATE imgui-ui::nfd)

if(WIN32)
    target_compile_definitions(imgui-ui-core PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        VC_EXTRALEAN
    )
endif()

if(IMGUI_UI_BUILD_SDL)
    file(GLOB IMGUI_UI_SDL_SOURCES CONFIGURE_DEPENDS "backends/sdl/*.cpp")
    list(APPEND IMGUI_UI_SDL_SOURCES ${IMGUI_UI_OPENGL_EFFECT_SOURCES})
    list(APPEND IMGUI_UI_SDL_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/backends/opengl/texture-loader.cpp"
        "${IMGUI_UI_IMGUI_BACKENDS_DIR}/imgui_impl_sdl3.cpp"
        "${IMGUI_UI_IMGUI_BACKENDS_DIR}/imgui_impl_opengl3.cpp"
    )

    imgui_ui_add_backend(sdl
        SOURCES ${IMGUI_UI_SDL_SOURCES}
        PRIVATE_INCLUDE_DIRS "${IMGUI_UI_IMGUI_BACKENDS_DIR}"
        LINK_LIBS
            imgui-ui::glad
            imgui-ui::lunasvg
            imgui-ui::nsgif
            SDL3::SDL3
            OpenGL::GL
    )

    imgui_ui_link_plutovg(imgui-ui-sdl)
endif()

if(IMGUI_UI_BUILD_RAYLIB)
    file(GLOB IMGUI_UI_RAYLIB_SOURCES CONFIGURE_DEPENDS "backends/raylib/*.cpp")
    list(APPEND IMGUI_UI_RAYLIB_SOURCES ${IMGUI_UI_OPENGL_EFFECT_SOURCES})
    list(APPEND IMGUI_UI_RAYLIB_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/backends/opengl/texture-loader.cpp"
        "${IMGUI_UI_IMGUI_BACKENDS_DIR}/imgui_impl_opengl3.cpp"
    )

    imgui_ui_add_backend(raylib
        SOURCES ${IMGUI_UI_RAYLIB_SOURCES}
        PRIVATE_INCLUDE_DIRS "${IMGUI_UI_IMGUI_BACKENDS_DIR}"
        LINK_LIBS
            raylib
            imgui-ui::glad
            imgui-ui::lunasvg
            imgui-ui::nsgif
    )

    imgui_ui_link_plutovg(imgui-ui-raylib)
endif()
