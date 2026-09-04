#include <ui/backends/raylib/backend.hpp>
#include <ui/tree/node.hpp>
#include <ui/ui.hpp>

#include "../demo.hpp"

#include <raylib.h>

#include <memory>
#include <utility>

int main() {
    // configure the demo before runtime construction because runtime owns the theme and asset registries.
    ui::RuntimeConfig runtime_config;
    configure_demo_runtime(runtime_config);
    ui::Runtime runtime(std::move(runtime_config));

    SetConfigFlags(FLAG_VSYNC_HINT);

    // create the backend & window
    auto backend = std::make_unique<ui::RaylibBackend>(ui::BackendConfig{
        .title = "imgui-ui raylib",
        .size = {1120.0F, 920.0F},
        .resizable = true,
    });

    UI surface(runtime, std::move(backend));

    // or just the backend
    // auto backend = std::make_unique<ui::RaylibBackend>();
    // ui surface(runtime, std::move(backend));
    if (!surface.ready()) return 1;

    setup_demo(surface, "raylib");
    while (!surface.is_done()) {
        // raylib input is not polled by the framework automatically, so forward it before update and draw each frame.
        ui::process_raylib_events(surface);

        surface.begin_input_frame();
        surface.begin_frame();
        surface.root().update(ImGui::GetIO().DeltaTime);
        surface.root().draw();
        surface.end_frame();
    }

    return 0;
}
