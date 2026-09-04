#pragma once

#include <SDL3/SDL_video.h>
#include <imgui.h>

#include <string>

namespace ui {
    class Window {
    public:
        Window(std::string title, ImVec2 size, SDL_WindowFlags flags);
        Window(SDL_Window* window, SDL_GLContext context);
        Window(const Window&) = delete;
        ~Window();

        Window& operator=(const Window&) = delete;

        bool valid() const;
        SDL_Window* handle() const;
        SDL_GLContext context() const;
        SDL_WindowID id() const;
        ImVec2 display_size() const;

        void make_current();
        void swap();

    private:
        SDL_Window* m_window = nullptr;
        SDL_GLContext m_context = nullptr;
        bool m_owned = true;
    };
} // namespace ui
