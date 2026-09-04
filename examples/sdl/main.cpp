#include <ui/backends/sdl/backend.hpp>
#include <ui/backends/sdl/debugger.hpp>
#include <ui/ui.hpp>

#include "../demo.hpp"

#include <SDL3/SDL.h>

#include <memory>
#include <utility>

int main() {
    // the framework does not initialize sdl, so the application must start its video subsystem first.
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    int result = 0;
    {
        // configure the demo before runtime construction because runtime owns the theme and asset registries.
        ui::RuntimeConfig runtime_config;
        configure_demo_runtime(runtime_config);
        ui::Runtime runtime(std::move(runtime_config));

        // create the backend & window
        auto backend = std::make_unique<ui::SdlBackend>(ui::BackendConfig{
            .title = "imgui-ui sdl",
            .size = {1120.0F, 920.0F},
            .resizable = true,
        });
        UI surface(runtime, std::move(backend));

        // or just the backend
        // auto backend = std::make_unique<ui::SdlBackend>(existing_window, existing_gl_context);
        // ui surface(runtime, std::move(backend));
        if (!surface.ready()) {
            result = 1;
        } else {
            setup_demo(surface, "sdl");

            ui::Debugger debugger(surface);
            debugger.setup();
            debugger.set_font("Inter Regular", ui::FONT_MEDIUM);
            debugger.set_hotkey(ImGuiMod_Shift | ImGuiKey_D);
            SDL_GL_SetSwapInterval(1);

            while (!surface.is_done()) {
                // the debugger gets first refusal so its shortcuts and controls do not reach the app surface.
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
                // render the debugger after the app because it uses a separate imgui context and window.
                debugger.render();
            }
        }
    }

    SDL_Quit();
    return result;
}
