#pragma once

#include "../layout/container.hpp"

#include <string>

namespace ui {
    class Texture;
    class ImageWidget;
    class TextWidget;

    /// edits a bound UTF-8 string through ImGui while exposing it as one retained, stylable input node.
    ///
    /// the widget owns focus routing and an optional decorative icon; ImGui handles text editing, selection, clipboard, and IME
    /// input.
    class TextInputWidget : public Container {
    public:
        TextInputWidget(std::string& value, std::string id = {});

        TextInputWidget& set_label(std::string label);
        TextInputWidget& set_label_placement(LabelPlacement placement);
        TextInputWidget& set_icon(Texture* icon);
        bool set_value(std::string value);

    protected:
        void apply_theme_defaults(const Theme& theme) override;
        void on_event(UiEvent& event) override;

    private:
        class FieldNode;

        void on_draw_end() override;
        void update_label_layout();

        std::string* m_value;
        TextWidget* m_label_node = nullptr;
        Container* m_input_node = nullptr;
        ImageWidget* m_icon_node = nullptr;
        FieldNode* m_field_node = nullptr;
        LabelPlacement m_label_placement = LabelPlacement::Inline;
        ImVec2 m_label_spacing{};
        bool m_focus_requested = false;
    };
} // namespace ui
