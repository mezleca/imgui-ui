#pragma once

#include "style.hpp"
#include "../layout/geometry.hpp"

#include <functional>

namespace ui {
    /// values available to a paint slot draw callback for the current node paint pass.
    struct PaintContext {
        Rect rect;
        Rect content_rect;
        ImDrawList& draw_list;
        const Style& style;
        float opacity;
    };

    /// a configurable layer rendered immediately before or after a styled node's contents.
    class PaintSlot final {
    public:
        using DrawCallback = std::function<void(const PaintContext&)>;

        PaintSlot(void* change_owner, Style::ChangeCallback change_callback);

        Style& style() {
            return m_style;
        }

        const Style& style() const {
            return m_style;
        }

        PaintSlot& set_draw_callback(DrawCallback callback);
        PaintSlot& set_opacity(float opacity);

    private:
        friend class StyledNode;

        void paint(Rect rect, Rect content_rect);

        Style m_style;
        DrawCallback m_draw_callback;
        float m_opacity = 1.0F;
    };
} // namespace ui
