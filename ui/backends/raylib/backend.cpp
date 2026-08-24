#include "backend.hpp"

#include "../../constants.hpp"
#include "../../imgui/context-scope.hpp"
#include "../../ui.hpp"

#include <imgui_impl_opengl3.h>
#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>

namespace ui {
    struct RaylibKeyMapping {
        int raylib_key;
        ImGuiKey imgui_key;
    };

    static constexpr std::array RAYLIB_KEYS = {
        RaylibKeyMapping{KEY_TAB, ImGuiKey_Tab},
        RaylibKeyMapping{KEY_LEFT, ImGuiKey_LeftArrow},
        RaylibKeyMapping{KEY_RIGHT, ImGuiKey_RightArrow},
        RaylibKeyMapping{KEY_UP, ImGuiKey_UpArrow},
        RaylibKeyMapping{KEY_DOWN, ImGuiKey_DownArrow},
        RaylibKeyMapping{KEY_PAGE_UP, ImGuiKey_PageUp},
        RaylibKeyMapping{KEY_PAGE_DOWN, ImGuiKey_PageDown},
        RaylibKeyMapping{KEY_HOME, ImGuiKey_Home},
        RaylibKeyMapping{KEY_END, ImGuiKey_End},
        RaylibKeyMapping{KEY_INSERT, ImGuiKey_Insert},
        RaylibKeyMapping{KEY_DELETE, ImGuiKey_Delete},
        RaylibKeyMapping{KEY_BACKSPACE, ImGuiKey_Backspace},
        RaylibKeyMapping{KEY_SPACE, ImGuiKey_Space},
        RaylibKeyMapping{KEY_ENTER, ImGuiKey_Enter},
        RaylibKeyMapping{KEY_ESCAPE, ImGuiKey_Escape},
        RaylibKeyMapping{KEY_APOSTROPHE, ImGuiKey_Apostrophe},
        RaylibKeyMapping{KEY_COMMA, ImGuiKey_Comma},
        RaylibKeyMapping{KEY_MINUS, ImGuiKey_Minus},
        RaylibKeyMapping{KEY_PERIOD, ImGuiKey_Period},
        RaylibKeyMapping{KEY_SLASH, ImGuiKey_Slash},
        RaylibKeyMapping{KEY_SEMICOLON, ImGuiKey_Semicolon},
        RaylibKeyMapping{KEY_EQUAL, ImGuiKey_Equal},
        RaylibKeyMapping{KEY_LEFT_BRACKET, ImGuiKey_LeftBracket},
        RaylibKeyMapping{KEY_BACKSLASH, ImGuiKey_Backslash},
        RaylibKeyMapping{KEY_RIGHT_BRACKET, ImGuiKey_RightBracket},
        RaylibKeyMapping{KEY_GRAVE, ImGuiKey_GraveAccent},
        RaylibKeyMapping{KEY_CAPS_LOCK, ImGuiKey_CapsLock},
        RaylibKeyMapping{KEY_SCROLL_LOCK, ImGuiKey_ScrollLock},
        RaylibKeyMapping{KEY_NUM_LOCK, ImGuiKey_NumLock},
        RaylibKeyMapping{KEY_PRINT_SCREEN, ImGuiKey_PrintScreen},
        RaylibKeyMapping{KEY_PAUSE, ImGuiKey_Pause},
        RaylibKeyMapping{KEY_KP_0, ImGuiKey_Keypad0},
        RaylibKeyMapping{KEY_KP_1, ImGuiKey_Keypad1},
        RaylibKeyMapping{KEY_KP_2, ImGuiKey_Keypad2},
        RaylibKeyMapping{KEY_KP_3, ImGuiKey_Keypad3},
        RaylibKeyMapping{KEY_KP_4, ImGuiKey_Keypad4},
        RaylibKeyMapping{KEY_KP_5, ImGuiKey_Keypad5},
        RaylibKeyMapping{KEY_KP_6, ImGuiKey_Keypad6},
        RaylibKeyMapping{KEY_KP_7, ImGuiKey_Keypad7},
        RaylibKeyMapping{KEY_KP_8, ImGuiKey_Keypad8},
        RaylibKeyMapping{KEY_KP_9, ImGuiKey_Keypad9},
        RaylibKeyMapping{KEY_KP_DECIMAL, ImGuiKey_KeypadDecimal},
        RaylibKeyMapping{KEY_KP_DIVIDE, ImGuiKey_KeypadDivide},
        RaylibKeyMapping{KEY_KP_MULTIPLY, ImGuiKey_KeypadMultiply},
        RaylibKeyMapping{KEY_KP_SUBTRACT, ImGuiKey_KeypadSubtract},
        RaylibKeyMapping{KEY_KP_ADD, ImGuiKey_KeypadAdd},
        RaylibKeyMapping{KEY_KP_ENTER, ImGuiKey_KeypadEnter},
        RaylibKeyMapping{KEY_KP_EQUAL, ImGuiKey_KeypadEqual},
        RaylibKeyMapping{KEY_LEFT_SHIFT, ImGuiKey_LeftShift},
        RaylibKeyMapping{KEY_LEFT_CONTROL, ImGuiKey_LeftCtrl},
        RaylibKeyMapping{KEY_LEFT_ALT, ImGuiKey_LeftAlt},
        RaylibKeyMapping{KEY_LEFT_SUPER, ImGuiKey_LeftSuper},
        RaylibKeyMapping{KEY_RIGHT_SHIFT, ImGuiKey_RightShift},
        RaylibKeyMapping{KEY_RIGHT_CONTROL, ImGuiKey_RightCtrl},
        RaylibKeyMapping{KEY_RIGHT_ALT, ImGuiKey_RightAlt},
        RaylibKeyMapping{KEY_RIGHT_SUPER, ImGuiKey_RightSuper},
        RaylibKeyMapping{KEY_KB_MENU, ImGuiKey_Menu},
        RaylibKeyMapping{KEY_ZERO, ImGuiKey_0},
        RaylibKeyMapping{KEY_ONE, ImGuiKey_1},
        RaylibKeyMapping{KEY_TWO, ImGuiKey_2},
        RaylibKeyMapping{KEY_THREE, ImGuiKey_3},
        RaylibKeyMapping{KEY_FOUR, ImGuiKey_4},
        RaylibKeyMapping{KEY_FIVE, ImGuiKey_5},
        RaylibKeyMapping{KEY_SIX, ImGuiKey_6},
        RaylibKeyMapping{KEY_SEVEN, ImGuiKey_7},
        RaylibKeyMapping{KEY_EIGHT, ImGuiKey_8},
        RaylibKeyMapping{KEY_NINE, ImGuiKey_9},
        RaylibKeyMapping{KEY_A, ImGuiKey_A},
        RaylibKeyMapping{KEY_B, ImGuiKey_B},
        RaylibKeyMapping{KEY_C, ImGuiKey_C},
        RaylibKeyMapping{KEY_D, ImGuiKey_D},
        RaylibKeyMapping{KEY_E, ImGuiKey_E},
        RaylibKeyMapping{KEY_F, ImGuiKey_F},
        RaylibKeyMapping{KEY_G, ImGuiKey_G},
        RaylibKeyMapping{KEY_H, ImGuiKey_H},
        RaylibKeyMapping{KEY_I, ImGuiKey_I},
        RaylibKeyMapping{KEY_J, ImGuiKey_J},
        RaylibKeyMapping{KEY_K, ImGuiKey_K},
        RaylibKeyMapping{KEY_L, ImGuiKey_L},
        RaylibKeyMapping{KEY_M, ImGuiKey_M},
        RaylibKeyMapping{KEY_N, ImGuiKey_N},
        RaylibKeyMapping{KEY_O, ImGuiKey_O},
        RaylibKeyMapping{KEY_P, ImGuiKey_P},
        RaylibKeyMapping{KEY_Q, ImGuiKey_Q},
        RaylibKeyMapping{KEY_R, ImGuiKey_R},
        RaylibKeyMapping{KEY_S, ImGuiKey_S},
        RaylibKeyMapping{KEY_T, ImGuiKey_T},
        RaylibKeyMapping{KEY_U, ImGuiKey_U},
        RaylibKeyMapping{KEY_V, ImGuiKey_V},
        RaylibKeyMapping{KEY_W, ImGuiKey_W},
        RaylibKeyMapping{KEY_X, ImGuiKey_X},
        RaylibKeyMapping{KEY_Y, ImGuiKey_Y},
        RaylibKeyMapping{KEY_Z, ImGuiKey_Z},
        RaylibKeyMapping{KEY_F1, ImGuiKey_F1},
        RaylibKeyMapping{KEY_F2, ImGuiKey_F2},
        RaylibKeyMapping{KEY_F3, ImGuiKey_F3},
        RaylibKeyMapping{KEY_F4, ImGuiKey_F4},
        RaylibKeyMapping{KEY_F5, ImGuiKey_F5},
        RaylibKeyMapping{KEY_F6, ImGuiKey_F6},
        RaylibKeyMapping{KEY_F7, ImGuiKey_F7},
        RaylibKeyMapping{KEY_F8, ImGuiKey_F8},
        RaylibKeyMapping{KEY_F9, ImGuiKey_F9},
        RaylibKeyMapping{KEY_F10, ImGuiKey_F10},
        RaylibKeyMapping{KEY_F11, ImGuiKey_F11},
        RaylibKeyMapping{KEY_F12, ImGuiKey_F12},
    };

    static Key core_key(int key) {
        switch (key) {
            case KEY_ESCAPE:
                return Key::Escape;
            case KEY_ENTER:
                return Key::Enter;
            case KEY_TAB:
                return Key::Tab;
            case KEY_LEFT:
                return Key::Left;
            case KEY_RIGHT:
                return Key::Right;
            case KEY_UP:
                return Key::Up;
            case KEY_DOWN:
                return Key::Down;
            default:
                return Key::Unknown;
        }
    }

    static std::string utf8_from_codepoint(int codepoint) {
        std::string result;
        if (codepoint <= 0x7f) {
            result.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7ff) {
            result.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        } else if (codepoint <= 0xffff) {
            result.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        } else {
            result.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        }
        return result;
    }

    static Color raylib_color(ImVec4 color) {
        return {
            static_cast<unsigned char>(std::clamp(color.x, 0.0F, 1.0F) * 255.0F),
            static_cast<unsigned char>(std::clamp(color.y, 0.0F, 1.0F) * 255.0F),
            static_cast<unsigned char>(std::clamp(color.z, 0.0F, 1.0F) * 255.0F),
            static_cast<unsigned char>(std::clamp(color.w, 0.0F, 1.0F) * 255.0F),
        };
    }

    static void dispatch_pointer(UI& surface, EventType type, ImVec2 position, PointerButton button = PointerButton::None) {
        UiEvent event = UiEvent::make(type);
        event.position = position;
        event.button = button;
        surface.dispatch(event);
    }

    RaylibBackend::RaylibBackend(Config config) : m_config(std::move(config)) {}

    RaylibBackend::~RaylibBackend() {
        if (m_owns_window && IsWindowReady()) CloseWindow();
    }

    bool RaylibBackend::initialize() {
        if (m_config.shared_with != nullptr) {
            TraceLog(LOG_ERROR, "ui: raylib does not support shared secondary windows");
            return false;
        }

        if (IsWindowReady()) {
            return true;
        }

        unsigned int flags = 0;
        if (m_config.resizable) flags |= FLAG_WINDOW_RESIZABLE;
        if (!m_config.visible) flags |= FLAG_WINDOW_HIDDEN;
        if (flags != 0) SetConfigFlags(flags);

        InitWindow(static_cast<int>(m_config.size.x), static_cast<int>(m_config.size.y), m_config.title.c_str());
        m_owns_window = IsWindowReady();
        return m_owns_window;
    }

    bool RaylibBackend::initialize_imgui() {
        m_imgui_initialized = ImGui_ImplOpenGL3_Init(nullptr);
        return m_imgui_initialized;
    }

    void RaylibBackend::shutdown_imgui() {
        if (!m_imgui_initialized) return;
        ImGui_ImplOpenGL3_Shutdown();
        m_imgui_initialized = false;
    }

    void RaylibBackend::make_current() {}

    void RaylibBackend::begin_frame(ImVec4 clear_color) {
        BeginDrawing();
        ClearBackground(raylib_color(clear_color));

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = display_size();
        const Vector2 scale = GetWindowScaleDPI();
        io.DisplayFramebufferScale = {scale.x, scale.y};
        io.DeltaTime = std::max(GetFrameTime(), 1.0F / 1000.0F);
        ImGui_ImplOpenGL3_NewFrame();
    }

    void RaylibBackend::render(ImDrawData* draw_data) {
        rlDrawRenderBatchActive();
        ImGui_ImplOpenGL3_RenderDrawData(draw_data);
        EndDrawing();
    }

    float RaylibBackend::content_scale() const {
        return IsWindowReady() ? GetWindowScaleDPI().x : 1.0F;
    }

    uint64_t RaylibBackend::window_id() const {
        return IsWindowReady() ? 1 : 0;
    }

    ImVec2 RaylibBackend::display_size() const {
        return IsWindowReady() ? ImVec2{static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())} : ImVec2{};
    }

    bool RaylibBackend::focused() const {
        return IsWindowReady() && IsWindowFocused();
    }

    void RaylibBackend::position_next_to(const Backend& target, float gap) {
        if (dynamic_cast<const RaylibBackend*>(&target) == nullptr || !IsWindowReady()) return;
        const Vector2 position = GetWindowPosition();
        SetWindowPosition(static_cast<int>(position.x + target.display_size().x + gap), static_cast<int>(position.y));
    }

    void RaylibBackend::show() {
        if (IsWindowReady()) ClearWindowState(FLAG_WINDOW_HIDDEN);
    }

    void RaylibBackend::hide() {
        if (IsWindowReady()) SetWindowState(FLAG_WINDOW_HIDDEN);
    }

    void RaylibBackend::raise() {
        if (!IsWindowReady()) return;
        ClearWindowState(FLAG_WINDOW_MINIMIZED);
        SetWindowFocused();
    }

    bool RaylibBackend::process_events(UI& surface) {
        if (WindowShouldClose()) {
            surface.exit();
            return true;
        }

        const ImGuiContextScope scope(surface.imgui_context());
        ImGuiIO& io = ImGui::GetIO();
        const Vector2 mouse = GetMousePosition();
        const ImVec2 mouse_position = {mouse.x, mouse.y};
        io.AddMousePosEvent(mouse.x, mouse.y);

        constexpr std::array mouse_buttons = {
            std::pair{MOUSE_BUTTON_LEFT, PointerButton::Left},
            std::pair{MOUSE_BUTTON_RIGHT, PointerButton::Right},
            std::pair{MOUSE_BUTTON_MIDDLE, PointerButton::Middle},
        };

        bool handled = false;
        for (std::size_t index = 0; index < mouse_buttons.size(); ++index) {
            const auto [raylib_button, core_button] = mouse_buttons[index];
            io.AddMouseButtonEvent(static_cast<int>(index), IsMouseButtonDown(raylib_button));
            if (IsMouseButtonPressed(raylib_button)) {
                dispatch_pointer(surface, EventType::PointerDown, mouse_position, core_button);
                handled = true;
            }
            if (IsMouseButtonReleased(raylib_button)) {
                dispatch_pointer(surface, EventType::PointerUp, mouse_position, core_button);
                handled = true;
            }
        }

        if (!m_has_mouse_position || mouse_position.x != m_previous_mouse_position.x ||
            mouse_position.y != m_previous_mouse_position.y) {
            dispatch_pointer(surface, EventType::PointerMove, mouse_position);
            m_previous_mouse_position = mouse_position;
            m_has_mouse_position = true;
            handled = true;
        }

        const Vector2 wheel = GetMouseWheelMoveV();
        if (wheel.x != 0.0F || wheel.y != 0.0F) {
            io.AddMouseWheelEvent(wheel.x * constants::SCROLL_WHEEL_SCALE, wheel.y * constants::SCROLL_WHEEL_SCALE);
            UiEvent event = UiEvent::make(EventType::Scroll);
            event.position = mouse_position;
            event.scroll = {wheel.x * constants::SCROLL_WHEEL_SCALE, wheel.y * constants::SCROLL_WHEEL_SCALE};
            surface.dispatch(event);
            handled = true;
        }

        io.AddKeyEvent(ImGuiMod_Ctrl, IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL));
        io.AddKeyEvent(ImGuiMod_Shift, IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
        io.AddKeyEvent(ImGuiMod_Alt, IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT));
        io.AddKeyEvent(ImGuiMod_Super, IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER));

        for (const RaylibKeyMapping& mapping : RAYLIB_KEYS) {
            io.AddKeyEvent(mapping.imgui_key, IsKeyDown(mapping.raylib_key));
            if (!IsKeyPressed(mapping.raylib_key) && !IsKeyReleased(mapping.raylib_key)) continue;

            UiEvent event = UiEvent::make(IsKeyPressed(mapping.raylib_key) ? EventType::KeyDown : EventType::KeyUp);
            event.key = core_key(mapping.raylib_key);
            surface.dispatch(event);
            handled = true;
        }

        for (int codepoint = GetCharPressed(); codepoint > 0; codepoint = GetCharPressed()) {
            io.AddInputCharacter(static_cast<unsigned int>(codepoint));
            UiEvent event = UiEvent::make(EventType::TextInput);
            event.text = utf8_from_codepoint(codepoint);
            surface.dispatch(event);
            handled = true;
        }

        return handled;
    }

    std::unique_ptr<Backend> create_raylib_backend(const Config& config) {
        return std::make_unique<RaylibBackend>(config);
    }

    bool process_raylib_events(UI& surface) {
        auto* backend = dynamic_cast<RaylibBackend*>(&surface.backend());
        return backend != nullptr && backend->process_events(surface);
    }
} // namespace ui
