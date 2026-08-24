#include <ui/backends/raylib/backend.hpp>
#include <ui/ui.hpp>

#include "../demo.hpp"

#include <filesystem>
#include <utility>

int main() {
    ui::set_backend(ui::create_raylib_backend);

    ui::RuntimeConfig runtime_config;
    runtime_config.theme.accent_color = {0.35F, 0.65F, 1.0F, 1.0F};
    runtime_config.theme.accent_hover_color = {0.55F, 0.78F, 1.0F, 1.0F};
    ui::Runtime runtime(std::move(runtime_config));
    const std::filesystem::path font = std::filesystem::path{IMGUI_UI_ASSETS_DIR} / "fonts/Inter.ttf";
    runtime.add_font(ui::FontType::REGULAR, font);
    runtime.add_font(ui::FontType::SEMIBOLD, font);
    runtime.add_font(ui::FontType::BOLD, font);

    UI surface(runtime, {.title = "imgui-ui raylib", .size = {900.0F, 600.0F}, .resizable = true});
    if (!surface.ready()) return 1;

    surface.set_primary_font(runtime.find_font(ui::FontType::REGULAR));
    surface.set_secondary_font(runtime.find_font(ui::FontType::SEMIBOLD));
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
