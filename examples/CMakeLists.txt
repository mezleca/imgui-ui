function(imgui_ui_add_example name)
    add_executable(imgui-ui-example-${name} ${name}/main.cpp demo.cpp demo.hpp)
    target_link_libraries(imgui-ui-example-${name} PRIVATE imgui-ui::${name})
    target_compile_definitions(imgui-ui-example-${name} PRIVATE
        IMGUI_UI_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets"
    )
endfunction()

if(IMGUI_UI_BUILD_SDL)
    imgui_ui_add_example(sdl)
endif()

if(IMGUI_UI_BUILD_RAYLIB)
    imgui_ui_add_example(raylib)
endif()
