#pragma once

#include "widget.hpp"
#include "text-value.hpp"

#include <cstdint>
#include <optional>
#include <string>

class UI;

namespace ui {
    class CheckboxVisualNode;

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

        StyledNode& frame();
        const StyledNode& frame() const;
        StyledNode& fill();
        const StyledNode& fill() const;

        bool changed() const override;
        bool on_draw() override;
        std::optional<std::string> content() const override;
        bool try_set_content(std::string content) override;

    private:
        void draw_children() override;
        void on_draw_end() override;
        void on_measure() override;

        UI& m_ui;
        bool* m_value;
        GenericValue m_label;
        CheckboxVisualNode* m_frame_node = nullptr;
        CheckboxVisualNode* m_fill_node = nullptr;
        ItemInputState m_input_state;
        ImVec2 m_restore_cursor{};
        CheckboxType m_type = CheckboxType::Standard;
        float m_box_size = 20.0F;
        bool m_changed = false;
    };
} // namespace ui
