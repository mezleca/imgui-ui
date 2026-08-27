#pragma once

#include "widget.hpp"
#include "text-value.hpp"

#include <concepts>
#include <optional>
#include <string>
#include <utility>
#include <variant>

class UI;

namespace ui {
    class NumberInputWidget final : public Widget {
        // imgui edits the original scalar through a typed pointer, while GenericValue mirrors it for text apis and metrics.
        using NumberValue = std::variant<
            char*, signed char*, unsigned char*, short*, unsigned short*, int*, unsigned int*, long*, unsigned long*, long long*,
            unsigned long long*, float*, double*>;

    public:
        template <typename T>
            requires std::constructible_from<NumberValue, T*>
        NumberInputWidget(UI& ui, T& value, std::string id = {})
            : Widget(std::move(id), "NumberInput"), m_ui(ui), m_value(value), m_number(&value),
              m_format(std::floating_point<T> ? "%.3f" : ""), m_speed(std::floating_point<T> ? 0.1F : 1.0F) {
            configure_default_styles();
        }

        NumberInputWidget& set_label(std::string label);
        NumberInputWidget& set_minimum(double minimum);
        NumberInputWidget& set_maximum(double maximum);
        NumberInputWidget& set_range(double minimum, double maximum);
        /// removes both bounds and restores unbounded drag behavior.
        NumberInputWidget& clear_range();
        NumberInputWidget& set_speed(float speed);
        NumberInputWidget& set_format(std::string format);
        NumberInputWidget& set_thumb_visible(bool visible);
        NumberInputWidget& set_thumb_size(float size);
        NumberInputWidget& set_thumb_color(ImColor color);

        bool changed() const override;
        bool on_draw() override;
        std::optional<std::string> content() const override;
        bool try_set_content(std::string content) override;

    private:
        template <typename T>
        bool draw_value(T& value);

        // keep the generic mirror current without making the imgui control operate on a converted copy.
        void sync_value() const;
        void configure_default_styles();
        void on_measure() override;

        UI& m_ui;
        mutable GenericValue m_value;
        NumberValue m_number;
        GenericValue m_label;
        std::string m_format;
        std::optional<double> m_minimum;
        std::optional<double> m_maximum;
        ImColor m_thumb_color;
        float m_speed;
        float m_thumb_size = 10.0F;
        bool m_thumb_visible = true;
        bool m_changed = false;
    };
} // namespace ui
