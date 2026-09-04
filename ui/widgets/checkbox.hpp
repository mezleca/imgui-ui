#pragma once

#include "widget.hpp"

#include <cstdint>
#include <string>

class UI;

namespace ui {
    class CheckboxVisualNode;
    class TextWidget;

    enum class CheckboxType : uint8_t {
        Standard,
        Radio,
    };

    class CheckboxWidget final : public Widget {
    public:
        CheckboxWidget(UI& ui, bool& value, std::string label, std::string id = {});

        CheckboxWidget& set_label(std::string label);
        CheckboxWidget& set_checked(bool checked);
        CheckboxWidget& set_type(CheckboxType type);
        CheckboxWidget& set_box_size(float size);
        CheckboxWidget& set_mark_color(ImColor color);

        StyledNode& frame();
        const StyledNode& frame() const;
        StyledNode& fill();
        const StyledNode& fill() const;

    protected:
        void apply_theme_defaults(const Theme& theme) override;

    private:
        bool paint() override;
        void on_measure() override;
        void arrange_children();
        Rect hit_rect(Rect visual_rect) const override;

        bool* m_value;
        CheckboxVisualNode* m_frame_node = nullptr;
        CheckboxVisualNode* m_fill_node = nullptr;
        TextWidget* m_label_node = nullptr;
        CheckboxType m_type = CheckboxType::Standard;
        float m_box_size = 20.0F;
    };
} // namespace ui
