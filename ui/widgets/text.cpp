#include "text.hpp"
#include "../imgui/draw.hpp"

#include <imgui.h>

using namespace ui;

void TextWidget::apply_theme_defaults(const Theme& theme) {
    configure_all_styles([&theme](Style& style) { style.color(theme.text_color); });
}

TextWidget& TextWidget::set_wrap(float width) {
    if (m_wrap != width) {
        m_wrap = width;
        m_text.set_wrap(width);
        invalidate_measure();
    }

    return *this;
}

TextWidget& TextWidget::set_overflow(TextOverflow overflow) {
    m_overflow = overflow;
    return *this;
}

bool TextWidget::empty() const {
    return m_text.str().empty();
}

TextWidget& TextWidget::set_text(std::string text) {
    if (text != m_text.str()) {
        m_text.set(std::move(text));
        invalidate_measure();
    }
    return *this;
}

void TextWidget::on_measure() {
    m_text.set_font(font());
    m_text.set_line_height(style().line_height());
    const ImVec2 padding = style().padding();
    const ImVec2 text_size = m_text.text_size();
    set_measured_size({text_size.x + padding.x * 2.0F, text_size.y + padding.y * 2.0F}, true, true);
}

bool TextWidget::paint() {
    const Style& current_style = style();
    const ImVec2 minimum = ImGui::GetCursorScreenPos();
    const Rect outer = Rect::from_position_size(minimum, layout().size());
    const Rect content = outer.inset(current_style.padding());

    draw_frame(outer, current_style);

    ImGui::Dummy(layout().size());
    const ImVec4 clip_rect = {content.min.x, content.min.y, content.max.x, content.max.y};

    if (m_wrap < 0.0F && m_overflow == TextOverflow::Ellipsis) {
        draw_text_ellipsis(content.min, current_style.color().get_col(), m_text, clip_rect);
    } else {
        draw_text(content.min, current_style.color().get_col(), m_text, m_wrap < 0.0F ? &clip_rect : nullptr);
    }

    return true;
}
