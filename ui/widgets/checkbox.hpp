#pragma once

#include "../layout/container.hpp"
#include "box.hpp"

#include <cstdint>
#include <string>

namespace ui {
    class TextWidget;

    enum class CheckboxType : uint8_t {
        Standard,
        Radio,
    };

    class CheckboxWidget : public Container {
    public:
        CheckboxWidget(bool& value, std::string label, std::string id = {});

        CheckboxWidget& set_label(std::string label);
        bool set_checked(bool checked);
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
        void on_click(UiEvent&) override;
        void input_state_changed() override;
        void on_update(float) override;
        Rect hit_rect(Rect visual_rect) const override;
        void update_mark_visibility();
        void update_shape();

        bool* m_value;
        BoxWidget* m_box_node = nullptr;
        BoxWidget* m_frame_node = nullptr;
        BoxWidget* m_fill_node = nullptr;
        TextWidget* m_label_node = nullptr;
        CheckboxType m_type = CheckboxType::Standard;
        float m_box_size = 20.0F;
        bool m_mark_visible = false;
    };
} // namespace ui
