add_executable(imgui-ui-tests
    core.cpp
    diagnostics.cpp
    input.cpp
    layout.cpp
    widgets.cpp
    imgui-context.hpp
)

target_link_libraries(imgui-ui-tests PRIVATE imgui-ui::core Catch2::Catch2WithMain)
target_compile_features(imgui-ui-tests PRIVATE cxx_std_20)
imgui_ui_enable_sanitizers(imgui-ui-tests)

include(Catch)
catch_discover_tests(imgui-ui-tests DISCOVERY_MODE PRE_TEST)
