#include "backend.hpp"
#include "window.hpp"

#include "../../constants.hpp"
#include "../../imgui/context-scope.hpp"
#include "../../imgui/opengl-blur.hpp"
#include "../../ui.hpp"

#include <glad/gl.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <SDL3/SDL_log.h>

#include <cfloat>
#include <optional>
#include <utility>

namespace ui {
    static GLADapiproc load_opengl(const char* name) {
        return reinterpret_cast<GLADapiproc>(SDL_GL_GetProcAddress(name));
    }

    static SDL_WindowID event_window_id(const SDL_Event& event) {
        if (event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST) {
            return event.window.windowID;
        }

        switch (event.type) {
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                return event.key.windowID;
            case SDL_EVENT_TEXT_INPUT:
                return event.text.windowID;
            case SDL_EVENT_MOUSE_MOTION:
                return event.motion.windowID;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                return event.button.windowID;
            case SDL_EVENT_MOUSE_WHEEL:
                return event.wheel.windowID;
            default:
                return 0;
        }
    }

    static PointerButton pointer_button(uint8_t button) {
        switch (button) {
            case SDL_BUTTON_LEFT:
                return PointerButton::Left;
            case SDL_BUTTON_RIGHT:
                return PointerButton::Right;
            case SDL_BUTTON_MIDDLE:
                return PointerButton::Middle;
            default:
                return PointerButton::None;
        }
    }

    static std::optional<int> imgui_mouse_button(PointerButton button) {
        switch (button) {
            case PointerButton::Left:
                return ImGuiMouseButton_Left;
            case PointerButton::Right:
                return ImGuiMouseButton_Right;
            case PointerButton::Middle:
                return ImGuiMouseButton_Middle;
            case PointerButton::None:
                return std::nullopt;
        }

        return std::nullopt;
    }

    static Key key_from_sdl(SDL_Keycode key) {
        switch (key) {
            case SDLK_ESCAPE:
                return Key::Escape;
            case SDLK_RETURN:
                return Key::Enter;
            case SDLK_TAB:
                return Key::Tab;
            case SDLK_LEFT:
                return Key::Left;
            case SDLK_RIGHT:
                return Key::Right;
            case SDLK_UP:
                return Key::Up;
            case SDLK_DOWN:
                return Key::Down;
            default:
                return Key::Unknown;
        }
    }

    static std::optional<UiEvent> event_from_sdl(const SDL_Event& event) {
        UiEvent result = UiEvent::make(EventType::Cancel);

        switch (event.type) {
            case SDL_EVENT_KEY_DOWN:
                result.type = EventType::KeyDown;
                result.key = key_from_sdl(event.key.key);
                break;
            case SDL_EVENT_KEY_UP:
                result.type = EventType::KeyUp;
                result.key = key_from_sdl(event.key.key);
                break;
            case SDL_EVENT_TEXT_INPUT:
                result.type = EventType::TextInput;
                result.text = event.text.text != nullptr ? event.text.text : "";
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                result.type = EventType::PointerDown;
                result.position = {event.button.x, event.button.y};
                result.button = pointer_button(event.button.button);
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                result.type = EventType::PointerUp;
                result.position = {event.button.x, event.button.y};
                result.button = pointer_button(event.button.button);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                result.type = EventType::PointerMove;
                result.position = {event.motion.x, event.motion.y};
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                result.type = EventType::Scroll;
                result.position = {event.wheel.mouse_x, event.wheel.mouse_y};
                result.scroll = {
                    event.wheel.x * constants::SCROLL_WHEEL_SCALE,
                    event.wheel.y * constants::SCROLL_WHEEL_SCALE,
                };
                break;
            default:
                return std::nullopt;
        }

        return result;
    }

    SdlBackend::SdlBackend(Config config) : m_config(std::move(config)) {}

    SdlBackend::SdlBackend(SDL_Window* window, SDL_GLContext context)
        : m_window(std::make_unique<Window>(window, context)), m_attached(true) {}

    bool SdlBackend::initialize() {
        if (m_attached) {
            return m_window != nullptr && m_window->valid();
        }

        Window* shared_window = nullptr;
        if (m_config.shared_with != nullptr) {
            auto* shared_backend = dynamic_cast<SdlBackend*>(m_config.shared_with);
            if (shared_backend == nullptr || shared_backend->m_window == nullptr) {
                SDL_Log("ui: cannot share a context with a different backend");
                return false;
            }
            shared_window = shared_backend->m_window.get();
        }

        SDL_WindowFlags flags = SDL_WINDOW_OPENGL;
        if (m_config.resizable) flags |= SDL_WINDOW_RESIZABLE;
        if (!m_config.visible) flags |= SDL_WINDOW_HIDDEN;

        m_window = std::make_unique<Window>(m_config.title, m_config.size, flags, shared_window);
        if (!m_window->valid()) {
            SDL_Log("ui: failed to create window '%s'", m_config.title.c_str());
            return false;
        }

        m_window->make_current();
        if (gladLoadGL(load_opengl) == 0 || !GLAD_GL_VERSION_3_3) {
            SDL_Log("ui: OpenGL 3.3 or newer is required");
            return false;
        }
        return true;
    }

    bool SdlBackend::initialize_imgui() {
        if (!ImGui_ImplSDL3_InitForOpenGL(m_window->handle(), m_window->context())) {
            return false;
        }

        if (!ImGui_ImplOpenGL3_Init(nullptr)) {
            ImGui_ImplSDL3_Shutdown();
            return false;
        }

        m_imgui_initialized = true;
        if (!initialize_opengl_blur()) {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplSDL3_Shutdown();
            m_imgui_initialized = false;
            return false;
        }
        return true;
    }

    void SdlBackend::shutdown_imgui() {
        if (!m_imgui_initialized) {
            return;
        }

        shutdown_opengl_blur();
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        m_imgui_initialized = false;
    }

    void SdlBackend::make_current() {
        if (m_window != nullptr) m_window->make_current();
    }

    void SdlBackend::begin_frame(ImVec4 clear_color) {
        begin_opengl_blur_frame();
        const ImVec2 size = display_size();
        if (!m_attached) {
            glViewport(0, 0, static_cast<int>(size.x), static_cast<int>(size.y));
            glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
    }

    void SdlBackend::render(ImDrawData* draw_data) {
        ImGui_ImplOpenGL3_RenderDrawData(draw_data);
        if (!m_attached) {
            m_window->swap();
        }
    }

    float SdlBackend::content_scale() const {
        return SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    }

    uint64_t SdlBackend::window_id() const {
        return m_window == nullptr ? 0 : m_window->id();
    }

    ImVec2 SdlBackend::display_size() const {
        return m_window == nullptr ? ImVec2{} : m_window->display_size();
    }

    bool SdlBackend::focused() const {
        return m_window != nullptr && SDL_GetKeyboardFocus() == m_window->handle();
    }

    void SdlBackend::position_next_to(const Backend& target, float gap) {
        if (m_attached) {
            return;
        }

        const auto* target_backend = dynamic_cast<const SdlBackend*>(&target);
        if (m_window == nullptr || target_backend == nullptr || target_backend->m_window == nullptr) {
            return;
        }

        int target_x = 0;
        int target_y = 0;
        int target_width = 0;
        SDL_GetWindowPosition(target_backend->m_window->handle(), &target_x, &target_y);
        SDL_GetWindowSize(target_backend->m_window->handle(), &target_width, nullptr);
        m_window->set_position(target_x + target_width + static_cast<int>(gap), target_y);
    }

    void SdlBackend::show() {
        if (!m_attached && m_window != nullptr) m_window->show();
    }

    void SdlBackend::hide() {
        if (!m_attached && m_window != nullptr) m_window->hide();
    }

    void SdlBackend::raise() {
        if (!m_attached && m_window != nullptr) m_window->raise();
    }

    bool SdlBackend::process_event(UI& surface, const SDL_Event& event) {
        if (event.type == SDL_EVENT_QUIT) {
            surface.exit();
            return true;
        }

        const SDL_WindowID window_id = event_window_id(event);
        if (window_id != 0 && window_id != m_window->id()) {
            return false;
        }

        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            surface.exit();
            return true;
        }

        const ImGuiContextScope scope(surface.imgui_context());
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            ImGui::GetIO().AddMousePosEvent(event.button.x, event.button.y);
        }

        const std::optional<UiEvent> translated = event_from_sdl(event);
        bool handled = false;
        if (translated.has_value()) {
            UiEvent dispatched = *translated;
            handled = surface.dispatch(dispatched);
        }

        if (translated.has_value() && handled) {
            const UiEvent& event = *translated;
            const bool pointer_blocked = (event.type == EventType::PointerMove || event.type == EventType::PointerDown ||
                                          event.type == EventType::PointerUp || event.type == EventType::Scroll) &&
                                         surface.input_router().pointer_blocked_at(event.position);
            if (event.type == EventType::PointerMove && pointer_blocked) {
                ImGui::GetIO().AddMousePosEvent(-FLT_MAX, -FLT_MAX);
                return true;
            }

            if ((event.type == EventType::PointerDown && handled) || event.type == EventType::PointerUp || pointer_blocked) {
                if (const std::optional<int> button = imgui_mouse_button(event.button); button.has_value()) {
                    ImGui::GetIO().AddMouseButtonEvent(*button, false);
                }
                return true;
            }

            if (event.type == EventType::Scroll) {
                return true;
            }
        }

        // imgui must receive state-changing events that the framework did not consume.
        SDL_Event imgui_event = event;
        if (imgui_event.type == SDL_EVENT_MOUSE_WHEEL) {
            imgui_event.wheel.x *= constants::SCROLL_WHEEL_SCALE;
            imgui_event.wheel.y *= constants::SCROLL_WHEEL_SCALE;
        }
        ImGui_ImplSDL3_ProcessEvent(&imgui_event);

        return handled;
    }

    std::unique_ptr<Backend> create_sdl_backend(const Config& config) {
        return std::make_unique<SdlBackend>(config);
    }

    std::unique_ptr<Backend> attach_sdl_backend(SDL_Window* window, SDL_GLContext context) {
        return std::make_unique<SdlBackend>(window, context);
    }

    bool process_sdl_event(UI& surface, const SDL_Event& event) {
        auto* backend = dynamic_cast<SdlBackend*>(&surface.backend());
        return backend != nullptr && backend->process_event(surface, event);
    }
} // namespace ui
