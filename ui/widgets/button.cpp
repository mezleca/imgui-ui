#include "button.hpp"
#include "../imgui/draw.hpp"
#include "../ui.hpp"
#include "../style/theme.hpp"

using namespace ui;

ButtonWidget::ButtonWidget(UI& ui, std::string text, LayoutSize size) : DrawListWidget({}, "Button"), m_text(text) {
    set_size(size);
    set_font(ui.get_primary_font(16));
    apply_theme_defaults(ui.theme());
}

void ButtonWidget::apply_theme_defaults(const Theme& theme) {
    configure_all_styles([&theme](Style& style) {
        style.color(theme.text_color)
            .background_color(theme.background_secondary_color)
            .border_color(theme.controls.border_color, 0.2F)
            .padding({12.0F, 6.0F})
            .border(BORDER_ALL)
            .border_radius(theme.controls.rounding)
            .border_thickness(theme.controls.border_thickness)
            .cursor(ImGuiMouseCursor_Hand);
    });

    configure_style(StyleType::ACTIVE, [&theme](Style& style) {
        style.background_color(theme.controls.active_color).border_color(theme.accent_color, 0.2F);
    });
    configure_style(StyleType::FOCUS, [&theme](Style& style) { style.border_color(theme.accent_color); });
    configure_style(StyleType::HOVER, [&theme](Style& style) { style.border_color(theme.accent_hover_color); });
}

ButtonWidget& ButtonWidget::set_text(std::string text) {
    if (text == m_text.str()) {
        return *this;
    }

    m_text.set(std::move(text));
    invalidate_measure();
    return *this;
}

ButtonWidget& ButtonWidget::set_on_click(std::function<void()> callback) {
    m_on_click = std::move(callback);
    return *this;
}

void ButtonWidget::dispatch_event(UiEvent& event) {
    Widget::dispatch_event(event);
    if (event.type != EventType::Click) {
        return;
    }

    animate()
        .background_color(style(StyleType::ACTIVE).background_color().value)
        .then(0.04F)
        .release_all({0.12F, easing::out_quad});

    if (m_on_click) {
        m_on_click();
    }
}

void ButtonWidget::paint_draw_list(ImDrawList& draw_list, Rect rect, const ComputedStyle& style) {
    const Rect content = rect.inset(style.padding());
    const ImVec2 text_size = ImGui::CalcTextSize(m_text.c_str());

    draw_frame(draw_list, rect, style);
    draw_text(
        draw_list,
        {
            content.min.x + (content.size().x - text_size.x) * m_text_alignment.x,
            content.min.y + (content.size().y - text_size.y) * m_text_alignment.y,
        },
        style.color().get_col(), m_text.str()
    );
}
