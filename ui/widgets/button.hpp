#pragma once

#include "text-value.hpp"
#include "widget.hpp"

#include <imgui.h>
#include <functional>
class UI;

namespace ui {
    class ButtonWidget : public DrawListWidget {
    public:
        ButtonWidget(UI& ui, std::string text, ImVec2 size = {100.0f, 60.0f});

        ButtonWidget& set_text_alignment(ImVec2 alignment) {
            m_text_alignment = alignment;
            return *this;
        }

        ButtonWidget& set_text(std::string text);
        ButtonWidget& on_click(std::function<void()> callback);

    private:
        void dispatch_event(UiEvent& event) override;
        void paint_draw_list(ImDrawList& draw_list, Rect rect, const Style& style) override;

        GenericValue m_text;
        std::function<void()> m_on_click;
        ImVec2 m_text_alignment{0.5F, 0.5F};
    };

} // namespace ui
