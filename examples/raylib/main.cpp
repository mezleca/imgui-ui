#include <ui/backends/raylib/backend.hpp>
#include <ui/backends/opengl/texture-loader.hpp>
#include <ui/tree/node.hpp>
#include <ui/ui.hpp>

#include "../demo.hpp"

#include <raylib.h>

#include <filesystem>
#include <memory>
#include <utility>

int main() {
    ui::RuntimeConfig runtime_config;
    runtime_config.theme.accent_color = {0.35F, 0.65F, 1.0F, 1.0F};
    runtime_config.theme.accent_hover_color = {0.55F, 0.78F, 1.0F, 1.0F};
    runtime_config.texture_loader = std::make_unique<ui::OpenGLTextureLoader>();
    ui::Runtime runtime(std::move(runtime_config));
    const std::filesystem::path font = std::filesystem::path{IMGUI_UI_ASSETS_DIR} / "fonts/Inter.ttf";
    runtime.fonts().add("Inter Regular", font);
    runtime.fonts().add("Inter SemiBold", font);
    runtime.fonts().add("Inter Bold", font);

    SetConfigFlags(FLAG_VSYNC_HINT);

    // standalone: the backend creates and owns the raylib window from its config.
    auto backend = std::make_unique<ui::RaylibBackend>(ui::BackendConfig{
        .title = "imgui-ui raylib",
        .size = {1120.0F, 920.0F},
        .resizable = true,
    });
    UI surface(runtime, std::move(backend));

    // attached: use the current raylib window instead.
    // auto backend = std::make_unique<ui::RaylibBackend>();
    // UI surface(runtime, std::move(backend));
    if (!surface.ready()) return 1;

    surface.set_primary_font(runtime.fonts().find("Inter Regular"));
    surface.set_secondary_font(runtime.fonts().find("Inter SemiBold"));
    setup_demo(surface, "raylib");
    while (!surface.is_done()) {
        ui::process_raylib_events(surface);
        surface.begin_input_frame();
        surface.begin_frame();
        surface.root().update(ImGui::GetIO().DeltaTime);
        surface.root().draw();
        surface.end_frame();
    }

    return 0;
}
