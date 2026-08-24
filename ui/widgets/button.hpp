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

        bool on_draw() override;
        std::optional<std::string> content() const override;
        bool try_set_content(std::string content) override;

    private:
        UI& m_ui;
        TextValue m_text;
    };

} // namespace ui
