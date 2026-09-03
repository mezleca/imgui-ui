#pragma once

#include "stack-container.hpp"

namespace ui {
    class ResizableContainer : public StackContainer {
    public:
        explicit ResizableContainer(std::string id);

        /// enables resizing along the selected axes from the bottom-right handle.
        ResizableContainer& set_resize(ResizeAxes resize);

    protected:
        Rect hit_rect(Rect visual_rect) const override;
        void on_draw_end() override;

    private:
        void handle_resize(UiEvent& event);
        void draw_resize_indicator();
        ImGuiMouseCursor resize_cursor() const;
        void update_resize_cursor() const;
        Rect resize_handle() const;

        ImVec2 m_drag_start = {0.0f, 0.0f};
        ImVec2 m_previous_size = {0.0f, 0.0f};
        ImVec2 m_parent_content_max{};
        bool m_dragging = false;
        ResizeAxes m_resize = ResizeAxes::None;
        ResizeAxes m_resizing = ResizeAxes::None;
    };
} // namespace ui
