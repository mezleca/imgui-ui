#include "paint-slot.hpp"

#include "../imgui/draw.hpp"

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

void PaintSlot::paint(Rect rect, Rect content_rect) {
    if (m_opacity <= 0.0F || !rect.valid() || ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    const PaintContext context{rect, content_rect, *ImGui::GetWindowDrawList(), m_style, m_opacity};
    if (m_draw_callback) {
        m_draw_callback(context);
        return;
    }

    draw_frame(context.rect, context.style, context.opacity);
}
