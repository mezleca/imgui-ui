#pragma once

#include "text-value.hpp"
#include "widget.hpp"

#include <imgui.h>
#include <functional>
class UI;

namespace ui {
    class ButtonWidget : public DrawListWidget {
    public:
        ButtonWidget(UI& ui, std::string text, LayoutSize size = {px(100.0F), px(60.0F)});

        ButtonWidget& set_text_alignment(ImVec2 alignment) {
            m_text_alignment = alignment;
            return *this;
        }

        ButtonWidget& set_text(std::string text);
        ButtonWidget& on_click(std::function<void()> callback);

    protected:
        void apply_theme_defaults(const Theme& theme) override;

    private:
        void dispatch_event(UiEvent& event) override;
        void paint_draw_list(ImDrawList& draw_list, Rect rect, const Style& style) override;

        GenericValue m_text;
        std::function<void()> m_on_click;
        ImVec2 m_text_alignment{0.5F, 0.5F};
    };

} // namespace ui
