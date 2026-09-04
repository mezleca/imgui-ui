file(GLOB_RECURSE IMGUI_UI_CORE_SOURCES CONFIGURE_DEPENDS "*.cpp")
list(FILTER IMGUI_UI_CORE_SOURCES EXCLUDE REGEX "/backends/")

add_library(imgui-ui-core STATIC ${IMGUI_UI_CORE_SOURCES})
add_library(imgui-ui::core ALIAS imgui-ui-core)
imgui_ui_enable_sanitizers(imgui-ui-core)

target_compile_features(imgui-ui-core PUBLIC cxx_std_20)
target_compile_definitions(imgui-ui-core PRIVATE "$<$<CONFIG:Debug>:OSU_STUFF_ENABLE_PROFILING=1>")
target_include_directories(imgui-ui-core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/.."
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../vendor/glad/include"
)
target_link_libraries(imgui-ui-core PUBLIC imgui)

if(WIN32)
    target_compile_definitions(imgui-ui-core PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        VC_EXTRALEAN
    )
endif()

if(IMGUI_UI_BUILD_SDL)
    file(GLOB IMGUI_UI_SDL_SOURCES CONFIGURE_DEPENDS "backends/sdl/*.cpp")
    list(APPEND IMGUI_UI_SDL_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/backends/opengl/texture-loader.cpp"
        "${IMGUI_UI_IMGUI_BACKENDS_DIR}/imgui_impl_sdl3.cpp"
        "${IMGUI_UI_IMGUI_BACKENDS_DIR}/imgui_impl_opengl3.cpp"
    )

    add_library(imgui-ui-sdl STATIC ${IMGUI_UI_SDL_SOURCES})
    add_library(imgui-ui::sdl ALIAS imgui-ui-sdl)
    imgui_ui_enable_sanitizers(imgui-ui-sdl)

    target_include_directories(imgui-ui-sdl PRIVATE "${IMGUI_UI_IMGUI_BACKENDS_DIR}")
    target_link_libraries(imgui-ui-sdl PUBLIC
        imgui-ui-core
        imgui-ui::glad
        SDL3::SDL3
        OpenGL::GL
        lunasvg
    )
endif()

if(IMGUI_UI_BUILD_RAYLIB)
    file(GLOB IMGUI_UI_RAYLIB_SOURCES CONFIGURE_DEPENDS "backends/raylib/*.cpp")
    list(APPEND IMGUI_UI_RAYLIB_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/backends/opengl/texture-loader.cpp"
        "${IMGUI_UI_IMGUI_BACKENDS_DIR}/imgui_impl_opengl3.cpp"
    )

    add_library(imgui-ui-raylib STATIC ${IMGUI_UI_RAYLIB_SOURCES})
    add_library(imgui-ui::raylib ALIAS imgui-ui-raylib)
    imgui_ui_enable_sanitizers(imgui-ui-raylib)

    target_include_directories(imgui-ui-raylib PRIVATE
        "${IMGUI_UI_IMGUI_BACKENDS_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../vendor/glad/include"
    )
    target_link_libraries(imgui-ui-raylib PUBLIC imgui-ui-core raylib lunasvg)
endif()
