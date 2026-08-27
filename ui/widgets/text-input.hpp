#pragma once

#include "../layout/stack-container.hpp"

#include <optional>
#include <string>

class IconTexture;
class UI;

namespace ui {
    class ImageWidget;

    class TextInputWidget final : public StackContainer {
    public:
        TextInputWidget(UI& ui, std::string& value);
        TextInputWidget(UI& ui, std::string& value, std::string label);

        TextInputWidget& set_icon(IconTexture* icon);

        bool changed() const override;
        const ItemInputState& input_state() const;
        std::optional<std::string> content() const override;
        bool try_set_content(std::string content) override;

    private:
        class FieldNode;

        void on_layout() override;
        void on_draw_end() override;

        UI& m_ui;
        std::string* m_value;
        ImageWidget* m_icon_node = nullptr;
        FieldNode* m_field_node = nullptr;
        ItemInputState m_input_state;
        bool m_focus_requested = false;
    };
} // namespace ui
