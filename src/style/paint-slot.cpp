#include <imgui-ui/style/paint-slot.hpp>

#include <algorithm>
#include <utility>

using namespace ui;

PaintSlot::PaintSlot(void* change_owner, Style::ChangeCallback change_callback) {
    m_style.set_change_callback(change_owner, change_callback);
}

PaintSlot& PaintSlot::set_draw_callback(DrawCallback callback) {
    m_draw_callback = std::move(callback);
    return *this;
}

PaintSlot& PaintSlot::set_opacity(float opacity) {
    m_opacity = std::clamp(opacity, 0.0F, 1.0F);
    return *this;
}

void PaintSlot::paint(Painter painter, Rect rect, Rect content_rect) {
    if (m_opacity <= 0.0F || !rect.valid()) {
        return;
    }

    const PaintContext context{rect, content_rect, painter, m_style};
    if (m_draw_callback) {
        m_draw_callback(context);
        return;
    }

    painter.frame(rect, m_style);
}
