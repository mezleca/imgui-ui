#pragma once

#include "../backend.hpp"

namespace ui {
    class UI;

    class RaylibBackend final : public Backend {
    public:
        /// uses the application's initialized raylib window.
        /// the application must close that window after UI destruction.
        RaylibBackend() = default;

        bool initialize() override;
        void process_events(UI& surface) override;
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
    private:
        void apply_mouse_cursor();

        bool m_imgui_initialized = false;
        ImGuiMouseCursor m_mouse_cursor = ImGuiMouseCursor_Arrow;
        ImVec2 m_pointer_position{};
        bool m_has_pointer_position = false;
    };
} // namespace ui
