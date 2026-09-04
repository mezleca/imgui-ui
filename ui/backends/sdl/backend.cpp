#include "backend.hpp"
#include "window.hpp"

#include "../../constants.hpp"
#include "../../imgui/context-scope.hpp"
#include "../../imgui/effects/blur/opengl.hpp"
#include "../../imgui/effects/shadow/opengl.hpp"
#include "../../ui.hpp"

#include <glad/gl.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <SDL3/SDL_log.h>

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

    static SDL_SystemCursor system_cursor(ImGuiMouseCursor cursor) {
        switch (cursor) {
            case ImGuiMouseCursor_TextInput:
                return SDL_SYSTEM_CURSOR_TEXT;
            case ImGuiMouseCursor_ResizeAll:
                return SDL_SYSTEM_CURSOR_MOVE;
            case ImGuiMouseCursor_ResizeNS:
                return SDL_SYSTEM_CURSOR_NS_RESIZE;
            case ImGuiMouseCursor_ResizeEW:
                return SDL_SYSTEM_CURSOR_EW_RESIZE;
            case ImGuiMouseCursor_ResizeNESW:
                return SDL_SYSTEM_CURSOR_NESW_RESIZE;
            case ImGuiMouseCursor_ResizeNWSE:
                return SDL_SYSTEM_CURSOR_NWSE_RESIZE;
            case ImGuiMouseCursor_Hand:
                return SDL_SYSTEM_CURSOR_POINTER;
            case ImGuiMouseCursor_Wait:
                return SDL_SYSTEM_CURSOR_WAIT;
            case ImGuiMouseCursor_Progress:
                return SDL_SYSTEM_CURSOR_PROGRESS;
            case ImGuiMouseCursor_NotAllowed:
                return SDL_SYSTEM_CURSOR_NOT_ALLOWED;
            default:
                return SDL_SYSTEM_CURSOR_DEFAULT;
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

    SdlBackend::SdlBackend(BackendConfig config) : Backend(std::move(config)) {}

    SdlBackend::SdlBackend(SDL_Window* window, SDL_GLContext context)
        : Backend(), m_window(std::make_unique<Window>(window, context)), m_attached(true) {}

    SdlBackend::~SdlBackend() {
        if (m_mouse_cursor != nullptr) {
            SDL_DestroyCursor(m_mouse_cursor);
        }
    }

    void SdlBackend::apply_mouse_cursor(ImGuiMouseCursor cursor) {
        if (cursor == ImGuiMouseCursor_None) {
            m_mouse_cursor_type = cursor;
            SDL_HideCursor();
            return;
        }

        if (cursor == m_mouse_cursor_type && m_mouse_cursor != nullptr) {
            SDL_ShowCursor();
            return;
        }

        SDL_Cursor* next_cursor = SDL_CreateSystemCursor(system_cursor(cursor));
        if (next_cursor == nullptr) {
            return;
        }

        SDL_SetCursor(next_cursor);
        SDL_ShowCursor();
        SDL_DestroyCursor(m_mouse_cursor);
        m_mouse_cursor = next_cursor;
        m_mouse_cursor_type = cursor;
    }

    void SdlBackend::set_mouse_cursor(ImGuiMouseCursor cursor) {
        m_mouse_cursor_type = cursor;
    }

    bool SdlBackend::initialize() {
        if (m_attached) {
            return m_window != nullptr && m_window->valid();
        }

        Window* shared_window = nullptr;
        if (config().shared_with != nullptr) {
            auto* shared_backend = dynamic_cast<SdlBackend*>(config().shared_with);
            if (shared_backend == nullptr || shared_backend->m_window == nullptr) {
                SDL_Log("ui: cannot share a context with a different backend");
                return false;
            }
            shared_window = shared_backend->m_window.get();
        }

        SDL_WindowFlags flags = SDL_WINDOW_OPENGL;
        if (config().resizable) flags |= SDL_WINDOW_RESIZABLE;
        if (!config().visible) flags |= SDL_WINDOW_HIDDEN;

        m_window = std::make_unique<Window>(config().title, config().size, flags, shared_window);
        if (!m_window->valid()) {
            SDL_Log("ui: failed to create window '%s'", config().title.c_str());
            return false;
        }

        m_window->make_current();
        if (!SDL_GL_SetSwapInterval(config().swap_interval)) {
            SDL_Log("ui: failed to set OpenGL swap interval: %s", SDL_GetError());
        }
        if (gladLoadGL(load_opengl) == 0 || !GLAD_GL_VERSION_3_3) {
            SDL_Log("ui: OpenGL 3.3 or newer is required");
            return false;
        }
        return true;
    }

    void SdlBackend::register_effects(EffectRegistry& effects) {
        register_opengl_blur(effects);
        register_opengl_box_shadow(effects);
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
        return true;
    }

    void SdlBackend::shutdown_imgui() {
        if (!m_imgui_initialized) {
            return;
        }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        m_imgui_initialized = false;
    }

    void SdlBackend::make_current() {
        if (m_window != nullptr) m_window->make_current();
    }

    void SdlBackend::begin_frame(ImVec4 clear_color) {
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
        apply_mouse_cursor(m_mouse_cursor_type);
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
        const std::optional<UiEvent> translated = event_from_sdl(event);

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            ImGui::GetIO().AddMousePosEvent(event.button.x, event.button.y);
        }

        bool handled = false;
        if (translated.has_value()) {
            UiEvent dispatched = *translated;
            handled = surface.dispatch(dispatched);
        }

        // imGui remains a drawing/input implementation detail for controls that
        // still need its text and scalar editing primitives.
        SDL_Event imgui_event = event;
        if (imgui_event.type == SDL_EVENT_MOUSE_WHEEL) {
            imgui_event.wheel.x *= constants::SCROLL_WHEEL_SCALE;
            imgui_event.wheel.y *= constants::SCROLL_WHEEL_SCALE;
        }

        ImGui_ImplSDL3_ProcessEvent(&imgui_event);
        return handled;
    }

    bool process_sdl_event(UI& surface, const SDL_Event& event) {
        auto* backend = dynamic_cast<SdlBackend*>(&surface.backend());
        return backend != nullptr && backend->process_event(surface, event);
    }
} // namespace ui
