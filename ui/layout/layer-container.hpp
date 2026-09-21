#pragma once

#include "container.hpp"

#include <cstdint>

namespace ui {
    enum class LayerMode : uint8_t {
        /// draws in the current imgui window and opens a child when scrolling or when padding, a background, a border, or an
        /// effect needs local state.
        Inline,
        /// draws in a separate imgui window that covers the layer's resolved bounds.
        Window,
    };

    class LayerContainer : public Container {
    public:
        explicit LayerContainer(std::string id, LayerMode mode = LayerMode::Inline);

    protected:
        LayerContainer(std::string id, LayerMode mode, std::string_view type_name);
        void resolve_layout() override;
        bool paint() override;
        void on_draw_end() override;
        ImGuiWindowFlags child_window_flags() const override;

    private:
        LayerMode m_mode;
        bool m_window_initialized = false;
        bool m_inline_child_window = false;

        bool paint_inline();
        bool paint_window();
    };
} // namespace ui
