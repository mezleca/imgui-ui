#include "decoration.hpp"

#include "../imgui/draw.hpp"

namespace ui {
    void Decoration::draw_for(Rect rect) {
        if (!visually_visible() || !rect.valid()) {
            return;
        }

        draw_frame(rect, style(), opacity());
    }
} // namespace ui
