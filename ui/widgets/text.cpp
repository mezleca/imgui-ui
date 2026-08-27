#include "text.hpp"

#include "../imgui/draw.hpp"

#include <imgui.h>

namespace ui {
    void TextWidget::set_wrap(float wrap) {
        if (m_wrap == wrap) {
            return;
        }

        m_wrap = wrap;
        m_text->set_wrap(wrap);
        invalidate_measure();
    }

    void TextWidget::on_measure() {
        m_text->set_font(font());
        const ImVec2 padding = style().padding();
        const ImVec2 text_size = m_text->text_size();
        set_size({text_size.x + padding.x * 2.0F, text_size.y + padding.y * 2.0F});
    }

    bool TextWidget::on_draw() {
        const Style& current_style = style();
        const ImVec2 padding = current_style.padding();
        const ImVec2 minimum = ImGui::GetCursorScreenPos();
        const Rect outer = Rect::from_position_size(minimum, layout().size());

        draw_frame(outer, current_style);
        ImGui::Dummy(layout().size());
        ImGui::GetWindowDrawList()->AddText(
            font(), ImGui::GetFontSize(), {minimum.x + padding.x, minimum.y + padding.y}, current_style.color().get_col(),
            m_text->c_str(), nullptr, m_wrap >= 0.0F ? m_wrap : 0.0F
        );

        return true;
    }

    std::optional<std::string> TextWidget::content() const {
        return m_text->str();
    }

    bool TextWidget::try_set_content(std::string content) {
        if (content == m_text->str()) {
            return false;
        }

        m_text->set(std::move(content));
        invalidate_measure();
        return true;
    }

} // namespace ui
