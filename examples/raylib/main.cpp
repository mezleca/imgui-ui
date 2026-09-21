#include <ui/backends/raylib/backend.hpp>
#include <ui/tree/node.hpp>
#include <ui/ui.hpp>
#include <ui/runtime.hpp>
#include "../demo.hpp"

#include <raylib.h>
#include <memory>
#include <utility>

using namespace ui;

int main() {
    // window configuration belongs to the application before the backend attaches to it.
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1120, 920, "imgui-ui raylib");

    {
        // configure the demo before runtime construction because runtime owns the theme and asset registries.
        RuntimeConfig runtime_config;
        configure_demo_runtime(runtime_config);
        Runtime runtime(std::move(runtime_config));

        // the backend only initializes imgui against the current raylib window.
        auto backend = std::make_unique<RaylibBackend>();
        UI surface(
            runtime, {
                         .backend = std::move(backend),
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
