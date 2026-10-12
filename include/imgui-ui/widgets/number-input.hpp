#pragma once

#include "widget.hpp"

#include <concepts>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace ui {
    class TextWidget;
    using NumberValue = std::variant<
        char*, signed char*, unsigned char*, short*, unsigned short*, int*, unsigned int*, long*, unsigned long*, long long*,
        unsigned long long*, float*, double*>;

    class NumberInputWidget : public Widget {
    public:
        template <typename T>
            requires std::constructible_from<NumberValue, T*>
        explicit NumberInputWidget(
            T& value, std::string label = {}, LabelPlacement placement = LabelPlacement::Inline, std::string id = {}
        )
            : Widget(std::move(id), "NumberInput"), m_number(&value), m_format(std::floating_point<T> ? "%.3f" : ""),
              m_speed(std::floating_point<T> ? 0.1F : 1.0F) {
            initialize_children();
            set_label(std::move(label), placement);
        }

        NumberInputWidget& set_label(std::string label, LabelPlacement placement = LabelPlacement::Inline);
        NumberInputWidget& set_minimum(double minimum);
        NumberInputWidget& set_maximum(double maximum);
        NumberInputWidget& set_range(double minimum, double maximum);
        NumberInputWidget& clear_range();
        NumberInputWidget& set_speed(float speed);
        NumberInputWidget& set_format(std::string format);
        NumberInputWidget& set_thumb_visible(bool visible);
        NumberInputWidget& set_thumb_size(float size);
        NumberInputWidget& set_thumb_color(Color color);

        template <typename T>
            requires std::constructible_from<NumberValue, T*>
        bool set_value(T value) {
            T* const* bound = std::get_if<T*>(&m_number);
            if (bound == nullptr || **bound == value) return false;

            **bound = value;
            notify_change();
            return true;
        }

    protected:
        void apply_theme_defaults(const Theme& theme) override;

    private:
        class FieldNode;

        void initialize_children();
        void update_label_layout();
        void value_changed();
        Rect hit_rect(Rect visual_rect) const override;
        void mouse_press_event(UiEvent& event) override;

        template <typename T>
        bool draw_value(T& value);

        NumberValue m_number;
        TextWidget* m_label = nullptr;
        FieldNode* m_field = nullptr;
        std::string m_format;
        std::optional<double> m_minimum = 0;
        std::optional<double> m_maximum = 100;
        Color m_thumb_color;
        LabelPlacement m_label_placement = LabelPlacement::Inline;
        ImVec2 m_label_spacing;
        float m_speed;
        float m_thumb_size = 10.0F;
        bool m_thumb_visible = true;
    };
} // namespace ui
