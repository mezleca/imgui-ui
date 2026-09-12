add_executable(imgui-ui-tests
    core.cpp
    diagnostics.cpp
    file-dialog.cpp
    input.cpp
    layout.cpp
    widgets.cpp
    imgui-context.hpp
)

target_link_libraries(imgui-ui-tests PRIVATE imgui-ui::core Catch2::Catch2WithMain)
target_compile_definitions(imgui-ui-tests PRIVATE IMGUI_UI_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../assets")
imgui_ui_enable_sanitizers(imgui-ui-tests)

include(Catch)
catch_discover_tests(imgui-ui-tests DISCOVERY_MODE PRE_TEST)

if(IMGUI_UI_BUILD_SDL)
    add_executable(imgui-ui-sdl-tests
        sdl-input.cpp
    )

    target_link_libraries(imgui-ui-sdl-tests PRIVATE imgui-ui::sdl Catch2::Catch2WithMain)
    imgui_ui_enable_sanitizers(imgui-ui-sdl-tests)

    catch_discover_tests(
        imgui-ui-sdl-tests
        DISCOVERY_MODE PRE_TEST
        PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
    )
endif()
