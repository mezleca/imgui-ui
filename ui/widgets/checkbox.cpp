#include "checkbox.hpp"
#include "../imgui/draw.hpp"
#include "../style/theme.hpp"
#include "../ui.hpp"

#include <algorithm>

namespace ui {
    CheckboxWidget::CheckboxWidget(UI& ui, bool& value, std::string label, std::string id)
        : Widget(std::move(id), "Checkbox"), m_ui(ui), m_value(&value), m_label(std::move(label)),
          m_mark_color(ui.theme().control_mark_color) {
        set_font(ui.get_primary_font(16));

        const Theme& theme = m_ui.theme();
        configure_all_styles([&theme](Style& style) {
            style.color(theme.text_color)
                .background_color(theme.control_background_color, 0.15F)
                .border_color(theme.control_border_color, 0.15F)
                .padding({4.0F, 4.0F})
                .border(BORDER_ALL)
                .border_radius(theme.checkbox_rounding)
                .border_thickness(theme.control_border_thickness);
        });
        configure_style(StyleType::HOVER, [&theme](Style& style) {
            style.background_color(theme.control_hover_color).border_color(theme.accent_hover_color);
        });
        configure_style(StyleType::ACTIVE, [&theme](Style& style) {
            style.background_color(theme.control_active_color).border_color(theme.accent_color);
        });
    }

    CheckboxWidget& CheckboxWidget::set_label(std::string label) {
        m_label = std::move(label);
        invalidate_measure();
        return *this;
    }

    CheckboxWidget& CheckboxWidget::set_type(CheckboxType type) {
        m_type = type;
        return *this;
    }

    CheckboxWidget& CheckboxWidget::set_box_size(float size) {
        m_box_size = std::max(1.0F, size);
        invalidate_measure();
        return *this;
    }

    CheckboxWidget& CheckboxWidget::set_mark_color(ImColor color) {
        m_mark_color = color;
        return *this;
    }

    void CheckboxWidget::on_measure() {
        ImFont* current_font = font();
        if (current_font == nullptr || ImGui::GetCurrentContext() == nullptr) {
            const ImVec2 padding = style().padding();
            set_size({m_box_size + padding.x * 2.0F, m_box_size + padding.y * 2.0F});
            return;
        }

        ImGui::PushFont(current_font);
        const ImVec2 padding = style().padding();
        const ImVec2 label_size = m_label.empty() ? ImVec2{} : ImGui::CalcTextSize(m_label.c_str());
        const float label_spacing = label_size.x > 0.0F ? ImGui::GetStyle().ItemInnerSpacing.x : 0.0F;
        ImGui::PopFont();

        set_size({
            m_box_size + label_spacing + label_size.x + padding.x * 2.0F,
            std::max(m_box_size, label_size.y) + padding.y * 2.0F,
        });
    }

    bool CheckboxWidget::changed() const {
        return m_changed;
    }

    bool CheckboxWidget::on_draw() {
        const Style& current_style = style();
        const ImVec2 padding = current_style.padding();
        const ImVec2 label_size = m_label.empty() ? ImVec2{} : ImGui::CalcTextSize(m_label.c_str());
        const float content_height = std::max(m_box_size, label_size.y);

        ImGui::PushID(this);
        m_changed = ImGui::InvisibleButton("##value", layout().size());
        if (m_changed) {
            *m_value = m_type == CheckboxType::Radio || !*m_value;
        }

        const ImVec2 outer_min = ImGui::GetItemRectMin();
        const Rect box = Rect::from_position_size(
            {outer_min.x + padding.x, outer_min.y + padding.y + (content_height - m_box_size) * 0.5F}, {m_box_size, m_box_size}
        );
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        if (m_type == CheckboxType::Radio) {
            const ImVec2 center = {(box.min.x + box.max.x) * 0.5F, (box.min.y + box.max.y) * 0.5F};
            const float radius = m_box_size * 0.5F;
            draw_list->AddCircleFilled(center, radius, current_style.background_color().get_col());
            if ((current_style.border() & BORDER_ALL) != 0 && current_style.border_thickness() > 0.0F) {
                draw_list->AddCircle(center, radius, current_style.border_color().get_col(), 0, current_style.border_thickness());
            }
            if (*m_value) {
                draw_list->AddCircleFilled(center, std::max(1.0F, radius - m_box_size / 6.0F), m_mark_color);
            }
        } else {
            draw_frame(box, current_style, *m_value ? m_mark_color : current_style.background_color().value);
        }

        if (!m_label.empty()) {
            draw_text(
                {box.max.x + ImGui::GetStyle().ItemInnerSpacing.x,
                 outer_min.y + padding.y + (content_height - label_size.y) * 0.5F},
                current_style.color().get_col(), m_label
            );
        }

        const ItemInputState input = m_ui.input().observe_item(*this);
        ImGui::PopID();

        apply_input_state(input);
        return true;
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
        return true;
    }
} // namespace ui
