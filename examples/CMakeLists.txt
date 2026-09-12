function(imgui_ui_add_example name)
    cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})

    add_executable(imgui-ui-example-${name} ${ARG_SOURCES})
    target_link_libraries(imgui-ui-example-${name} PRIVATE imgui-ui::${name})
    target_compile_definitions(imgui-ui-example-${name} PRIVATE
        IMGUI_UI_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets"
    )
    imgui_ui_enable_sanitizers(imgui-ui-example-${name})
endfunction()

if(IMGUI_UI_BUILD_SDL)
    imgui_ui_add_example(sdl SOURCES sdl/main.cpp demo.cpp demo.hpp)
endif()

if(IMGUI_UI_BUILD_RAYLIB)
    imgui_ui_add_example(raylib SOURCES raylib/main.cpp demo.cpp demo.hpp)
endif()
