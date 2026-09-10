#pragma once

#include "text-value.hpp"
#include "widget.hpp"

#include <functional>
#include <imgui.h>
#include <utility>

namespace ui {
    class UI;
    class ButtonWidget : public DrawListWidget {
    public:
        ButtonWidget(UI& ui, std::string text, LayoutSize size = {px(100.0F), px(60.0F)});

        ButtonWidget& set_text_alignment(ImVec2 alignment) {
            m_text_alignment = alignment;
            return *this;
        }

        ButtonWidget& set_text(std::string text);
        /// runs after the click animation starts.
        ButtonWidget& set_on_click(std::function<void()> callback) {
            m_on_click = std::move(callback);
            return *this;
        }

    protected:
        void apply_theme_defaults(const Theme& theme) override;
        void on_click(UiEvent&) override;

    private:
        void paint_draw_list(ImDrawList& draw_list, Rect rect, const ComputedStyle& style) override;

        GenericValue m_text;
        std::function<void()> m_on_click;
        ImVec2 m_text_alignment{0.5F, 0.5F};
    };

} // namespace ui
