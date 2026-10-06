#include <imgui-ui/widgets/text.hpp>
#include <imgui-ui/imgui/painter.hpp>

#include <imgui.h>

using namespace ui;

void TextWidget::apply_theme_defaults(const Theme& theme) {
    configure_all_styles([&theme](Style& style) { style.color(theme.text_color); });
}

TextWidget& TextWidget::set_wrap(float width) {
    if (m_text.wrap_width() != width) {
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
    if (m_text.set(std::move(text))) {
        invalidate_measure();
    }

    return *this;
}

void TextWidget::on_measure() {
    m_text.set_font(font());
    m_text.set_line_height(computed_style().line_height());
    const ImVec2 text_size = m_text.text_size();
    set_measured_content_size(text_size, true, true);
}

bool TextWidget::paint() {
    const ComputedStyle& current_style = computed_style();
    const ImVec2 minimum = ImGui::GetCursorScreenPos();
    const Rect outer = Rect::from_position_size(minimum, layout().size());
    const Rect content = content_rect(outer);

    const Painter paint = painter();
    paint.frame(outer, current_style);

    ImGui::Dummy(layout().size());
    const ImVec4 clip_rect = {content.min.x, content.min.y, content.max.x, content.max.y};

    if (m_text.wrap_width() < 0.0F && m_overflow == TextOverflow::Ellipsis) {
        paint.text_ellipsis(content.min, current_style.color().value, m_text, clip_rect);
    } else {
        paint.text(content.min, current_style.color().value, m_text, m_text.wrap_width() < 0.0F ? &clip_rect : nullptr);
    }

    return true;
}
