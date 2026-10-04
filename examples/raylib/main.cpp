#include <imgui-ui/backends/raylib/backend.hpp>
#include <imgui-ui/tree/node.hpp>
#include <imgui-ui/surface.hpp>
#include <imgui-ui/runtime.hpp>
#include "../demo.hpp"

#include <raylib.h>
#include <memory>
#include <utility>

using namespace ui;

int main() {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1120, 920, "imgui-ui raylib");

    // destroy the surface before destroying the application-owned graphics context.
    {
        RuntimeConfig runtime_config;
        configure_demo_runtime(runtime_config);
        Runtime runtime(std::move(runtime_config));

        Surface surface(
            runtime, {
                         .backend = std::make_unique<RaylibBackend>(),
                         .enable_debugger = true,
                     }
        );

        setup_demo(surface, "raylib");

        while (!surface.is_done()) {
            surface.process_events();

            surface.frame();
        }
    }

    CloseWindow();
    return 0;
}
