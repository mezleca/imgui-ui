#include <ui/backends/sdl/backend.hpp>
#include <ui/tree/node.hpp>
#include <ui/ui.hpp>
#include <ui/runtime.hpp>
#include "../demo.hpp"

#include <SDL3/SDL.h>
#include <memory>
#include <utility>

using namespace ui;

int main() {
    // all my homies hate xwayland
#if defined(__linux__)
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");
#endif

    // the framework does not initialize sdl, so the application must start its video subsystem first.
    if (!SDL_Init(SDL_INIT_VIDEO)) return 1;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    // window configuration belongs to the application before the backend attaches to it.
    SDL_Window* window = SDL_CreateWindow("imgui-ui sdl", 1120, 920, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (window == nullptr) {
        SDL_Quit();
        return 1;
    }

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (context == nullptr) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(1);

    {
        // configure the demo before runtime construction because runtime owns the theme and asset registries.
        RuntimeConfig runtime_config;
        configure_demo_runtime(runtime_config);
        Runtime runtime(std::move(runtime_config));

        // the backend only initializes imgui against this user-owned window and context.
        auto backend = std::make_unique<SdlBackend>(window, context);
        UI surface(
            runtime, {
                         .backend = std::move(backend),
                         .enable_debugger = true,
                     }
        );

        setup_demo(surface, "sdl");

        while (!surface.is_done()) {
            surface.process_events();

            surface.frame();
        }
    }

    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
