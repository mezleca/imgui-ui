#include "checkbox.hpp"
#include "../imgui/draw.hpp"
#include "../style/theme.hpp"
#include "../ui.hpp"

#include <algorithm>

using namespace ui;

class ui::CheckboxVisualNode final : public StyledNode {
public:
    CheckboxVisualNode(std::string id, bool* value, bool fill, CheckboxType type)
        : StyledNode(std::move(id), "CheckboxVisual"), m_value(value), m_fill(fill), m_type(type) {}

    void set_type(CheckboxType type) {
        m_type = type;
    }

private:
    bool on_draw() override {
        if (m_fill && !*m_value) {
            return true;
        }

        const Style& current_style = style();
        const Rect rect = m_fill ? layout().screen_rect().inset(current_style.padding()) : layout().screen_rect();

        if (!rect.valid()) {
            return true;
        }

        if (m_type == CheckboxType::Radio) {
            const ImVec2 size = rect.size();
            const ImVec2 center = {(rect.min.x + rect.max.x) * 0.5F, (rect.min.y + rect.max.y) * 0.5F};
            const float radius = std::min(size.x, size.y) * (m_fill ? 1.0F / 3.0F : 0.5F);
            draw_circle(center, radius, current_style.background_color().get_col());
            if (!m_fill && (current_style.border() & BORDER_ALL) != 0 && current_style.border_thickness() > 0.0F) {
                draw_circle_outline(center, radius, current_style.border_color().get_col(), current_style.border_thickness());
            }
        } else {
            draw_frame(rect, current_style);
        }

        return true;
    }

    bool* m_value = nullptr;
    bool m_fill;
    CheckboxType m_type;
};

CheckboxWidget::CheckboxWidget(UI& ui, bool& value, std::string label, std::string id)
    : Widget(std::move(id), "Checkbox"), m_ui(ui), m_value(&value), m_label(std::move(label)) {
    const Theme& theme = m_ui.theme();
    set_font(ui.get_primary_font(16));

    m_frame_node = &add_child<CheckboxVisualNode>("frame", nullptr, false, m_type);
    m_fill_node = &add_child<CheckboxVisualNode>("fill", &value, true, m_type);
    m_frame_node->set_enabled(false);
    m_fill_node->set_enabled(false);

    configure_all_styles([&theme](Style& style) { style.color(theme.text_color).padding({4.0F, 4.0F}); });

    m_frame_node->configure_all_styles([&theme](Style& style) {
        style.background_color(theme.control_background_color, 0.15F)
            .border_color(theme.control_border_color, 0.15F)
            .border(BORDER_ALL)
            .border_radius(theme.checkbox_rounding)
            .border_thickness(theme.control_border_thickness);
    });

    m_fill_node->configure_all_styles([&theme](Style& style) {
        style.background_color(theme.control_mark_color).border_radius(theme.checkbox_rounding);
    });

    m_frame_node->configure_style(StyleType::HOVER, [&theme](Style& style) {
        style.background_color(theme.control_hover_color).border_color(theme.accent_hover_color);
    });

    m_frame_node->configure_style(StyleType::ACTIVE, [&theme](Style& style) {
        style.background_color(theme.control_active_color).border_color(theme.accent_color);
    });
}

CheckboxWidget& CheckboxWidget::set_label(std::string label) {
    if (label == m_label.str()) {
        return *this;
    }

    m_label.set(std::move(label));
    invalidate_measure();
    return *this;
}

CheckboxWidget& CheckboxWidget::set_type(CheckboxType type) {
    if (m_type == type) {
        return *this;
    }

    m_type = type;
    m_frame_node->set_type(type);
    m_fill_node->set_type(type);
    return *this;
}

CheckboxWidget& CheckboxWidget::set_box_size(float size) {
    m_box_size = std::max(1.0F, size);
    invalidate_measure();
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

void CheckboxWidget::on_measure() {
    ImFont* current_font = font();
    if (current_font == nullptr || ImGui::GetCurrentContext() == nullptr) {
        const ImVec2 padding = style().padding();
        set_size({m_box_size + padding.x * 2.0F, m_box_size + padding.y * 2.0F});
        return;
    }

    const ImVec2 padding = style().padding();
    m_label.set_font(current_font);
    const ImVec2 label_size = m_label.text_size();
    const float label_spacing = label_size.x > 0.0F ? ImGui::GetStyle().ItemInnerSpacing.x : 0.0F;

    set_size({
        m_box_size + label_spacing + label_size.x + padding.x * 2.0F,
        std::max(m_box_size, label_size.y) + padding.y * 2.0F,
    });
}

bool CheckboxWidget::changed() const {
    return m_changed;
}

bool CheckboxWidget::on_draw() {
    ImGui::PushID(this);

    m_changed = ImGui::InvisibleButton("##value", layout().size());
    if (m_changed) {
        *m_value = m_type == CheckboxType::Radio || !*m_value;
        notify_change();
    }

    m_input_state = update_input(m_ui.input());
    m_restore_cursor = ImGui::GetCursorPos();
    m_frame_node->set_interaction_style(m_input_state.hovered, m_input_state.active, m_input_state.focused);
    m_fill_node->set_interaction_style(m_input_state.hovered, m_input_state.active, m_input_state.focused);

    return true;
}

void CheckboxWidget::draw_children() {
    const ImVec2 widget_padding = style().padding();
    const Rect widget_rect = layout().screen_rect();
    const ImVec2 frame_position = {widget_rect.min.x + widget_padding.x, widget_rect.min.y + widget_padding.y};
    const ImVec2 frame_size = {m_box_size, m_box_size};
    const ImVec2 frame_padding = m_frame_node->style().padding();
    const float border_inset = m_frame_node->style().border() != BORDER_NONE ? m_frame_node->style().border_thickness() : 0.0F;
    const ImVec2 fill_inset = {frame_padding.x + border_inset, frame_padding.y + border_inset};
    const ImVec2 fill_size = {
        std::max(0.0F, frame_size.x - fill_inset.x * 2.0F),
        std::max(0.0F, frame_size.y - fill_inset.y * 2.0F),
    };

    draw_child_at_screen(*m_frame_node, frame_size, frame_position);
    draw_child_at_screen(*m_fill_node, fill_size, {frame_position.x + fill_inset.x, frame_position.y + fill_inset.y});

    const Style& current_style = style();
    m_label.set_font(font());
    const ImVec2 label_size = m_label.text_size();
    const float content_height = std::max(m_box_size, label_size.y);
    if (label_size.x > 0.0F) {
        const Rect& frame_rect = m_frame_node->layout().screen_rect();
        draw_text(
            {frame_rect.max.x + ImGui::GetStyle().ItemInnerSpacing.x,
             layout().screen_rect().min.y + widget_padding.y + (content_height - label_size.y) * 0.5F},
            current_style.color().get_col(), m_label.str()
        );
    }
}

void CheckboxWidget::on_draw_end() {
    // child nodes use absolute positions, so restore the flow cursor before submitting the widget bounds.
    ImGui::SetCursorPos(m_restore_cursor);
    ImGui::Dummy({});
    ImGui::PopID();
}

std::optional<std::string> CheckboxWidget::content() const {
    return *m_value ? "true" : "false";
}

bool CheckboxWidget::try_set_content(std::string content) {
    bool value = false;
    if (content == "true" || content == "1") {
        value = true;
    } else if (content != "false" && content != "0") {
        return false;
    }

    if (*m_value == value) {
        return false;
    }

    *m_value = value;
    notify_change();
    return true;
}
