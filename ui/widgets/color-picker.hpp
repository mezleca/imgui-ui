#pragma once

#include "../layout/stack-container.hpp"

#include <string>

namespace ui {
    class ColorPickerPopup;
    class ColorPickerPreviewNode;
    class TextWidget;
    class UI;

    /// edits an rgba value through a preview and a compact hsv popup.
    class ColorPickerWidget final : public StackContainer {
    public:
        ColorPickerWidget(UI& ui, ImColor& color, std::string label = {}, std::string id = {});

        ColorPickerWidget& set_label(std::string label);
        bool set_color(ImColor color);
        void open();
        void close();

        bool is_open() const {
            return m_open;
        }

        TextWidget& label();
        Widget& preview();
        Widget& popup();

    protected:
        void apply_theme_defaults(const Theme& theme) override;

    private:
        friend class ColorPickerPopup;

        ImColor* m_color = nullptr;
        TextWidget* m_label_node = nullptr;
        ColorPickerPreviewNode* m_preview = nullptr;
        ColorPickerPopup* m_popup = nullptr;
        bool m_open = false;
    };
} // namespace ui
