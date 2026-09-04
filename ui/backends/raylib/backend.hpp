#pragma once

#include "../backend.hpp"

class UI;

namespace ui {
    class RaylibBackend final : public Backend {
    public:
        explicit RaylibBackend(BackendConfig config);
        RaylibBackend();
        ~RaylibBackend() override;

        bool initialize() override;
        void register_effects(EffectRegistry& effects) override;
        bool initialize_imgui() override;
        void shutdown_imgui() override;
        void begin_frame(ImVec4 clear_color) override;
        void set_mouse_cursor(ImGuiMouseCursor cursor) override;
        void render(ImDrawData* draw_data) override;
        float content_scale() const override;
        uint64_t window_id() const override;
        ImVec2 display_size() const override;

        /// forwards the current raylib input state to the retained tree.
        bool process_events(UI& surface);

    private:
        void apply_mouse_cursor();

        bool m_attached = false;
        bool m_owns_window = false;
        bool m_imgui_initialized = false;
        ImGuiMouseCursor m_mouse_cursor = ImGuiMouseCursor_Arrow;
        ImVec2 m_pointer_position{};
        bool m_has_pointer_position = false;
    };

    bool process_raylib_events(UI& surface);
} // namespace ui
