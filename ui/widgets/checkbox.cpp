#include "checkbox.hpp"
#include "../style/theme.hpp"
#include "../ui.hpp"
#include "text.hpp"

#include <algorithm>

using namespace ui;

CheckboxWidget::CheckboxWidget(bool& value, std::string label, std::string id)
    : Container(std::move(id), StackDirection::Horizontal), m_value(&value) {
    set_type_name("Checkbox");
    set_size({fit(), fit()});
    set_input_mode(InputMode::Target);
    set_content_alignment(Anchor::CenterLeft);

    m_box_node = &add<BoxWidget>("box", LayoutSize{px(m_box_size), px(m_box_size)});
    m_frame_node = &m_box_node->add<BoxWidget>("frame", LayoutSize{grow(), grow()});
    m_fill_node = &m_frame_node->add<BoxWidget>("fill", LayoutSize{grow(), grow()});
    m_label_node = &add<TextWidget>(std::move(label));
    m_mark_visible = *m_value;
    m_fill_node->set_visible(m_mark_visible);
}

void CheckboxWidget::apply_theme_defaults(const Theme& theme) {
    Container::apply_theme_defaults(theme);
    set_font(surface().get_primary_font(16));
    set_spacing(theme.metrics.item_inner_spacing.x);
    m_label_node->configure_all_styles([&theme](Style& style) { style.color(theme.text_color).padding({0.0F, 2.0F}); });

    configure_all_styles([&theme](Style& style) { style.color(theme.text_color).padding({4.0F, 4.0F}); });

    m_frame_padding = theme.controls.border_thickness;
    m_frame_node->configure_all_styles([&theme](Style& style) { style.control(theme); });

    m_fill_node->configure_all_styles([&theme](Style& style) {
        style.background_color(theme.controls.mark_color).border(BORDER_NONE);
    });

    m_frame_node->configure_style(StyleType::HOVER, [&theme](Style& style) {
        style.background_color(theme.controls.hover_color).border_color(theme.accent_hover_color);
    });

    m_frame_node->configure_style(StyleType::ACTIVE, [&theme](Style& style) {
        style.background_color(theme.controls.active_color).border_color(theme.accent_color);
    });

    update_shape();
}

CheckboxWidget& CheckboxWidget::set_label(std::string label) {
    m_label_node->set_text(std::move(label));
    return *this;
}

bool CheckboxWidget::set_checked(bool checked) {
    if (*m_value == checked) {
        return false;
    }

    *m_value = checked;
    update_mark_visibility();
    notify_change();
    return true;
}

void CheckboxWidget::on_click(UiEvent& event) {
    if (event.button != PointerButton::Left) {
        return;
    }

    *m_value = m_type == CheckboxType::Radio || !*m_value;
    update_mark_visibility();
    m_frame_node->animate()
        .background_color(m_frame_node->style(StyleType::ACTIVE).background_color().value)
        .then(0.04F)
        .release_all({0.12F, easing::out_quad});
    notify_change();
}

CheckboxWidget& CheckboxWidget::set_type(CheckboxType type) {
    if (m_type == type) {
        return *this;
    }

    m_type = type;
    update_shape();
    return *this;
}

CheckboxWidget& CheckboxWidget::set_box_size(float size) {
    const float resolved = std::max(1.0F, size);
    if (m_box_size == resolved) {
        return *this;
    }

    m_box_size = resolved;
    m_box_node->set_size({px(m_box_size), px(m_box_size)});
    update_shape();
    return *this;
}

CheckboxWidget& CheckboxWidget::set_mark_color(ImColor color) {
    m_fill_node->configure_all_styles([color](Style& style) { style.background_color(color); });
    return *this;
}

StyledNode& CheckboxWidget::frame() {
    return *m_frame_node;
}

const StyledNode& CheckboxWidget::frame() const {
    return *m_frame_node;
}

StyledNode& CheckboxWidget::fill() {
    return *m_fill_node;
}

const StyledNode& CheckboxWidget::fill() const {
    return *m_fill_node;
}

void CheckboxWidget::input_state_changed() {
    Container::input_state_changed();
    const InputState& state = input_state();
    m_frame_node->set_interaction_style(state.hovered, state.active, state.focused);
    m_fill_node->set_interaction_style(state.hovered, state.active, state.focused);
}

void CheckboxWidget::on_update(float) {
    update_mark_visibility();
}

Rect CheckboxWidget::hit_rect(Rect) const {
    return m_frame_node->layout().visual_rect();
}

void CheckboxWidget::update_mark_visibility() {
    const bool visible = *m_value;
    if (m_mark_visible == visible) {
        return;
    }

    m_mark_visible = visible;
    m_fill_node->set_visible(visible);
}

void CheckboxWidget::update_shape() {
    const float radius = m_type == CheckboxType::Radio ? m_box_size * 0.5F : 2.0F;
    m_frame_node->configure_all_styles([radius](Style& style) { style.border_radius(radius); });
    m_fill_node->configure_all_styles([radius](Style& style) { style.border_radius(radius); });

    const float inset = std::max(m_frame_padding, m_box_size * 0.20F);
    m_frame_node->configure_all_styles([inset](Style& style) { style.padding({inset, inset}); });
}
