#include <ui/backends/sdl/backend.hpp>
#include <ui/backends/sdl/debugger.hpp>
#include <ui/backends/sdl/icon.hpp>
#include <ui/ui.hpp>

#include "../demo.hpp"

#include <SDL3/SDL.h>

#include <filesystem>
#include <memory>
#include <utility>

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    int result = 0;
    {
        ui::RuntimeConfig runtime_config;
        runtime_config.theme.accent_color = {0.35F, 0.65F, 1.0F, 1.0F};
        runtime_config.theme.accent_hover_color = {0.55F, 0.78F, 1.0F, 1.0F};
        runtime_config.icon_loader = ui::make_sdl_icon_loader();
        ui::Runtime runtime(std::move(runtime_config));
        const std::filesystem::path font = std::filesystem::path{IMGUI_UI_ASSETS_DIR} / "fonts/Inter.ttf";
        runtime.fonts().add("Inter Regular", font);
        runtime.fonts().add("Inter SemiBold", font);
        runtime.fonts().add("Inter Bold", font);

        // standalone: the backend creates and owns its sdl window and opengl context.
        auto backend = std::make_unique<ui::SdlBackend>(ui::BackendConfig{
            .title = "imgui-ui sdl",
            .size = {1120.0F, 920.0F},
            .resizable = true,
        });
        UI surface(runtime, std::move(backend));

        // attached: use an existing sdl window and opengl context instead.
        // auto backend = std::make_unique<ui::SdlBackend>(existing_window, existing_gl_context);
        // UI surface(runtime, std::move(backend));
        if (!surface.ready()) {
            result = 1;
        } else {
            surface.set_primary_font(runtime.fonts().find("Inter Regular"));
            surface.set_secondary_font(runtime.fonts().find("Inter SemiBold"));
            setup_demo(surface, "sdl");

            ui::Debugger debugger(surface);
            debugger.setup();
            debugger.set_font("Inter Regular", ui::FONT_MEDIUM);
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
