#include "checkbox.hpp"
#include "../imgui/draw.hpp"
#include "../style/theme.hpp"
#include "../ui.hpp"
#include "text.hpp"

#include <algorithm>

using namespace ui;

class ui::CheckboxVisualNode final : public DrawListWidget {
public:
    CheckboxVisualNode(std::string id, bool* value, bool fill, CheckboxType type)
        : DrawListWidget(std::move(id), "CheckboxVisual", false), m_value(value), m_fill(fill), m_type(type) {}

    void set_type(CheckboxType type) {
        m_type = type;
    }

private:
    void paint_draw_list(ImDrawList& draw_list, Rect rect, const ComputedStyle& current_style) override {
        if (m_fill && !*m_value) {
            return;
        }

        if (m_fill) {
            rect = rect.inset(current_style.padding());
        }
        if (!rect.valid()) {
            return;
        }

        if (m_type == CheckboxType::Radio) {
            const ImVec2 size = rect.size();
            const ImVec2 center = {(rect.min.x + rect.max.x) * 0.5F, (rect.min.y + rect.max.y) * 0.5F};
            const float radius = std::min(size.x, size.y) * (m_fill ? 1.0F / 3.0F : 0.5F);
            draw_circle(draw_list, center, radius, current_style.background_color().get_col());
            if (!m_fill && (current_style.border() & BORDER_ALL) != 0 && current_style.border_thickness() > 0.0F) {
                draw_circle_outline(
                    draw_list, center, radius, current_style.border_color().get_col(), current_style.border_thickness()
                );
            }
        } else {
            draw_frame(draw_list, rect, current_style);
        }
    }

    bool* m_value = nullptr;
    bool m_fill;
    CheckboxType m_type;
};

CheckboxWidget::CheckboxWidget(UI& ui, bool& value, std::string label, std::string id)
    : Widget(std::move(id), "Checkbox"), m_value(&value) {
    set_font(ui.get_primary_font(16));

    m_frame_node = &add<CheckboxVisualNode>("frame", nullptr, false, m_type);
    m_fill_node = &add<CheckboxVisualNode>("fill", &value, true, m_type);
    m_label_node = &add<TextWidget>(std::move(label));
    apply_theme_defaults(ui.theme());

    _on_event = [this](UiEvent& event) {
        if (event.type != EventType::Click || event.button != PointerButton::Left) {
            return;
        }

        *m_value = m_type == CheckboxType::Radio || !*m_value;
        notify_change();
    };
}

void CheckboxWidget::apply_theme_defaults(const Theme& theme) {
    m_label_node->configure_all_styles([&theme](Style& style) { style.color(theme.text_color); });

    configure_all_styles([&theme](Style& style) { style.color(theme.text_color).padding({4.0F, 4.0F}); });

    m_frame_node->configure_all_styles([&theme](Style& style) {
        style.control(theme, {}).border_radius(theme.checkbox_rounding);
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
    m_label_node->set_text(std::move(label));
    return *this;
}

CheckboxWidget& CheckboxWidget::set_checked(bool checked) {
    if (*m_value != checked) {
        *m_value = checked;
        notify_change();
    }
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
    const float resolved = std::max(1.0F, size);
    if (m_box_size == resolved) {
        return *this;
    }

    m_box_size = resolved;
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
        set_measured_size({m_box_size + padding.x * 2.0F, m_box_size + padding.y * 2.0F}, true, true);
        return;
    }

    const ImVec2 padding = style().padding();
    const ImVec2 label_size = m_label_node->layout().intrinsic_size();
    const float label_spacing = label_size.x > 0.0F ? ImGui::GetStyle().ItemInnerSpacing.x : 0.0F;

    set_measured_size(
        {
            m_box_size + label_spacing + label_size.x + padding.x * 2.0F,
            std::max(m_box_size, label_size.y) + padding.y * 2.0F,
        },
        true, true
    );
}

bool CheckboxWidget::paint() {
    arrange_children();
    ImGui::Dummy(layout().size());

    const InputState& state = input_state();
    m_frame_node->set_interaction_style(state.hovered, state.active, state.focused);
    m_fill_node->set_interaction_style(state.hovered, state.active, state.focused);
    return true;
}

void CheckboxWidget::arrange_children() {
    const ImVec2 widget_padding = style().padding();
    const ImVec2 frame_size = {m_box_size, m_box_size};
    const Rect& parent_content = layout().parent_content_rect();
    const ImVec2 frame_offset = {
        layout().local_rect().min.x - parent_content.min.x + widget_padding.x,
        layout().local_rect().min.y - parent_content.min.y + widget_padding.y,
    };
    const ImVec2 frame_padding = m_frame_node->style().padding();
    const float border_inset = m_frame_node->style().border() == BORDER_NONE ? 0.0F : m_frame_node->style().border_thickness();
    const ImVec2 fill_inset = {frame_padding.x + border_inset, frame_padding.y + border_inset};
    const ImVec2 fill_size = {
        std::max(0.0F, frame_size.x - fill_inset.x * 2.0F),
        std::max(0.0F, frame_size.y - fill_inset.y * 2.0F),
    };

    arrange_child(*m_frame_node, frame_size, {.offset = frame_offset});
    arrange_child(
        *m_fill_node, fill_size,
        {.offset = {
             frame_offset.x + fill_inset.x,
             frame_offset.y + fill_inset.y,
         }}
    );

    const ImVec2 label_size = m_label_node->layout().intrinsic_size();
    const float label_spacing = label_size.x > 0.0F ? ImGui::GetStyle().ItemInnerSpacing.x : 0.0F;
    arrange_child(
        *m_label_node, label_size,
        {.offset = {frame_offset.x + frame_size.x + label_spacing, frame_offset.y + (frame_size.y - label_size.y) * 0.5F}}
    );
}

Rect CheckboxWidget::hit_rect(Rect visual_rect) const {
    const ImVec2 padding = style().padding();
    const ImVec2 available = visual_rect.size();
    const ImVec2 box_size = {
        std::min(m_box_size, std::max(0.0F, available.x - padding.x * 2.0F)),
        std::min(m_box_size, std::max(0.0F, available.y - padding.y * 2.0F)),
    };
    return Rect::from_position_size({visual_rect.min.x + padding.x, visual_rect.min.y + padding.y}, box_size);
}
