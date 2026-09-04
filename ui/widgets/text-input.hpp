#pragma once

#include "../layout/stack-container.hpp"

#include <string>

class UI;

namespace ui {
    class Texture;
    class ImageWidget;

    class TextInputWidget final : public StackContainer {
    public:
        TextInputWidget(UI& ui, std::string& value);
        TextInputWidget(UI& ui, std::string& value, std::string label);

        TextInputWidget& set_icon(Texture* icon);
        bool set_value(std::string value);

    private:
        class FieldNode;

        void on_measure() override;
        void on_draw_end() override;
        UI& m_ui;
        std::string* m_value;
        ImageWidget* m_icon_node = nullptr;
        FieldNode* m_field_node = nullptr;
        bool m_focus_requested = false;
    };
} // namespace ui
