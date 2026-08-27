#pragma once

#include "widget.hpp"

#include <cstdint>
#include <optional>
#include <string>

class UI;

namespace ui {
    enum class CheckboxType : uint8_t {
        Standard,
        Radio,
    };

    class CheckboxWidget final : public Widget {
    public:
        CheckboxWidget(UI& ui, bool& value, std::string label, std::string id = {});

        CheckboxWidget& set_label(std::string label);
        CheckboxWidget& set_type(CheckboxType type);
        CheckboxWidget& set_box_size(float size);
        CheckboxWidget& set_mark_color(ImColor color);

        bool changed() const override;
        bool on_draw() override;
        std::optional<std::string> content() const override;
        bool try_set_content(std::string content) override;

    private:
        void on_measure() override;

        UI& m_ui;
        bool* m_value;
        std::string m_label;
        ImColor m_mark_color;
        CheckboxType m_type = CheckboxType::Standard;
        float m_box_size = 20.0F;
        bool m_changed = false;
    };
} // namespace ui
