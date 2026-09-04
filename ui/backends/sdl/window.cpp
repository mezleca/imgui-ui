#include "window.hpp"

#include <SDL3/SDL_log.h>
#include <imgui.h>

using namespace ui;
Window::Window(std::string title, ImVec2 size, SDL_WindowFlags flags) {
    m_window = SDL_CreateWindow(title.c_str(), size.x, size.y, flags);

    if (m_window == nullptr) {
        SDL_Log("SDL_CreateWindow(%s): %s", title.c_str(), SDL_GetError());
        return;
    }

    m_context = SDL_GL_CreateContext(m_window);

    if (m_context == nullptr) {
        SDL_Log("SDL_GL_CreateContext(%s): %s", title.c_str(), SDL_GetError());
        SDL_DestroyWindow(m_window);

        m_window = nullptr;
    }
}

Window::~Window() {
    if (!m_owned) {
        return;
    }

    if (m_context != nullptr) SDL_GL_DestroyContext(m_context);
    if (m_window != nullptr) SDL_DestroyWindow(m_window);
}

Window::Window(SDL_Window* window, SDL_GLContext context) : m_window(window), m_context(context), m_owned(false) {}

bool Window::valid() const {
    return m_window != nullptr && m_context != nullptr;
}

SDL_Window* Window::handle() const {
    return m_window;
}

SDL_GLContext Window::context() const {
    return m_context;
}

SDL_WindowID Window::id() const {
    return m_window == nullptr ? 0 : SDL_GetWindowID(m_window);
}

ImVec2 Window::display_size() const {
    if (m_window == nullptr) {
        return {};
    }

    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(m_window, &width, &height);
    return {static_cast<float>(width), static_cast<float>(height)};
}

void Window::make_current() {
    if (valid()) {
        SDL_GL_MakeCurrent(m_window, m_context);
    }
}

void Window::swap() {
    if (m_window != nullptr) {
        SDL_GL_SwapWindow(m_window);
    }
}
