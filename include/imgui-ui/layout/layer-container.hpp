#pragma once

#include "container.hpp"

#include <cstdint>

namespace ui {
    enum class LayerMode : uint8_t {
        /// draws in a child window inside the parent's imgui window.
        Inline,

        /// draws in a separate imgui window. defaults to the main viewport's work area.
        Window,

        /// draws a regular imgui window at a screen position, constrained to the viewport work area.
        /// it remains open alongside native imgui popups.
        Popup,
    };

    class LayerContainer : public Container {
    public:
        /// starts out of flow with top-left placement and grow/grow sizing. inline mode fills the parent's content area
        /// and compensates for parent scrolling, keeping anchored children stationary in the parent's viewport.
        /// window and popup modes create separate imgui windows and resolve unassigned size against the viewport work area.
        /// defaults to InputMode::None, allowing input through empty areas. Blocker also occludes native controls behind
        /// an active inline layer, while its descendants remain interactive. hiding or disabling the layer removes that barrier.
        explicit LayerContainer(std::string id, LayerMode mode = LayerMode::Inline);

        /// sets the screen position used by popup mode before viewport clamping.
        void set_popup_position(ImVec2 position);

    protected:
        LayerContainer(std::string id, LayerMode mode, std::string_view type_name);
        void on_layout() override;
        bool paint() override;
        void on_draw_end() override;
        ImGuiWindowFlags child_window_flags() const override;
        virtual ImGuiWindowFlags window_flags() const;

    private:
        bool paint_inline();
        bool paint_window();

        LayerMode m_mode;
        ImVec2 m_popup_position;
        bool m_window_initialized = false;
    };
} // namespace ui
