#pragma once

#include "widget.hpp"
#include "text-value.hpp"

#include <imgui.h>
#include <optional>

class UI;

namespace ui {
    class ButtonWidget : public Widget {
    public:
        ButtonWidget(UI& ui, std::string text, ImVec2 size = {100.0f, 60.0f});

        ButtonWidget& set_text_alignment(ImVec2 alignment) {
            m_text_alignment = alignment;
            return *this;
        }

        bool on_draw() override;
        std::optional<std::string> content() const override;
        bool try_set_content(std::string content) override;

    private:
        UI& m_ui;
        TextValue m_text;
        ImVec2 m_text_alignment{0.5F, 0.5F};
    };

} // namespace ui
