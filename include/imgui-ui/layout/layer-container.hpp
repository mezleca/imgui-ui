#pragma once

#include "container.hpp"

#include <cstdint>

namespace ui {
    enum class LayerMode : uint8_t {
        /// draws in the parent's imgui window. opens a child inside scrollable parents or when its styling needs one.
        Inline,
        /// draws in a separate imgui window. defaults to the main viewport's work area.
        Window,
    };

    class LayerContainer : public Container {
    public:
        /// defaults to inline mode and top-left placement. without an explicit size, inline mode fills the parent's
        /// content area and window mode uses the main viewport's work area.
        /// defaults to InputMode::None so empty areas pass input through to underlying nodes.
        explicit LayerContainer(std::string id, LayerMode mode = LayerMode::Inline);

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
        bool m_window_initialized = false;
        bool m_inline_child_window = false;
    };
} // namespace ui
