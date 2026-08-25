#include <ui/backends/sdl/backend.hpp>
#include <ui/backends/sdl/debugger.hpp>
#include <ui/ui.hpp>

#include "../demo.hpp"

#include <SDL3/SDL.h>

#include <filesystem>
#include <utility>

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    ui::set_backend(ui::create_sdl_backend);
    int result = 0;
    {
        ui::RuntimeConfig runtime_config;
        runtime_config.theme.accent_color = {0.35F, 0.65F, 1.0F, 1.0F};
        runtime_config.theme.accent_hover_color = {0.55F, 0.78F, 1.0F, 1.0F};
        ui::Runtime runtime(std::move(runtime_config));
        const std::filesystem::path font = std::filesystem::path{IMGUI_UI_ASSETS_DIR} / "fonts/Inter.ttf";
        runtime.add_font(ui::FontType::REGULAR, font);
        runtime.add_font(ui::FontType::SEMIBOLD, font);
        runtime.add_font(ui::FontType::BOLD, font);

        // standalone: the surface creates and owns its sdl window and opengl context.
        UI surface(runtime, {.title = "imgui-ui sdl", .size = {900.0F, 600.0F}, .resizable = true});

        // attached: use an existing sdl window and opengl context instead.
        // auto backend = ui::attach_sdl_backend(existing_window, existing_gl_context);
        // UI surface(runtime, std::move(backend));
        if (!surface.ready()) {
            result = 1;
        } else {
            surface.set_primary_font(runtime.find_font(ui::FontType::REGULAR));
            surface.set_secondary_font(runtime.find_font(ui::FontType::SEMIBOLD));
            setup_demo(surface, "sdl");

            ui::Debugger debugger(surface);
            debugger.setup();
            debugger.set_font(ui::FontType::REGULAR, ui::FONT_MEDIUM);
            debugger.set_hotkey(ImGuiMod_Shift | ImGuiKey_D);
            SDL_GL_SetSwapInterval(1);

            while (!surface.is_done()) {
                SDL_Event event;
                while (SDL_PollEvent(&event)) {
                    if (!debugger.process_sdl_event(&event)) ui::process_sdl_event(surface, event);
                }

                surface.begin_input_frame();
                surface.begin_frame();
                const float dt = ImGui::GetIO().DeltaTime;
                debugger.update(dt);
                surface.root().update(dt);
                surface.root().draw();
                debugger.draw_highlight();
                surface.end_frame();
                debugger.render();
            }
        }
    }

    SDL_Quit();
    return result;
}
