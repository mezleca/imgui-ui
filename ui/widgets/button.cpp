#include "button.hpp"
#include "../imgui/draw.hpp"
#include "../ui.hpp"
#include "../style/theme.hpp"

namespace ui {
    ButtonWidget::ButtonWidget(UI& ui, std::string text, ImVec2 size) : Widget({}, "Button"), m_ui(ui), m_text(text) {
        set_size(size);
        set_font(ui.get_primary_font(16));

        const ui::Theme& theme = m_ui.theme();
        configure_all_styles([&theme](Style& style) {
            style.color(theme.text_color)
                .background_color(theme.background_secondary_color)
                .border_color(theme.background_secondary_color, 0.2F)
                .padding({12.0F, 6.0F})
                .border(BORDER_ALL)
                .border_radius(4.0F)
                .border_thickness(2.0F);
        });

        configure_style(StyleType::ACTIVE, [&theme](Style& style) { style.border_color(theme.accent_color, 0.2F); });

        configure_style(StyleType::HOVER, [&theme](Style& style) { style.border_color(theme.border_color); });
    }

    std::optional<std::string> ButtonWidget::content() const {
        return m_text.str();
    }

    bool ButtonWidget::try_set_content(std::string content) {
        if (content == m_text.str()) {
            return false;
        }

        m_text.set(std::move(content));
        return true;
    }

    bool ButtonWidget::on_draw() {
        const Style& style = this->style();

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, m_text_alignment);
        const bool pressed = ImGui::Button(m_text.c_str(), layout().size());
        ImGui::PopStyleVar();

        const ItemInputState input = m_ui.input().observe_item(*this);
        apply_input_state(input, pressed && !input.blocked);

        draw_border({ImGui::GetItemRectMin(), ImGui::GetItemRectMax()}, style);
        return true;
    }

} // namespace ui
