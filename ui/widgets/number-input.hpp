#pragma once

#include "widget.hpp"
#include "text-value.hpp"

#include <concepts>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

class UI;

namespace ui {
    class NumberInputWidget final : public Widget {
        // imgui edits the original scalar through its typed pointer.
        using NumberValue = std::variant<
            char*, signed char*, unsigned char*, short*, unsigned short*, int*, unsigned int*, long*, unsigned long*, long long*,
            unsigned long long*, float*, double*>;

    public:
        template <typename T>
            requires std::constructible_from<NumberValue, T*>
        NumberInputWidget(UI& ui, T& value, std::string id = {})
            : Widget(std::move(id), "NumberInput"), m_value(value), m_number(&value),
              m_format(std::floating_point<T> ? "%.3f" : ""), m_speed(std::floating_point<T> ? 0.1F : 1.0F) {
            apply_theme_defaults(ui.theme());
        }

        NumberInputWidget& set_label(std::string label);
        NumberInputWidget& set_minimum(double minimum);
        NumberInputWidget& set_maximum(double maximum);
        NumberInputWidget& set_range(double minimum, double maximum);
        NumberInputWidget& clear_range();
        NumberInputWidget& set_speed(float speed);
        NumberInputWidget& set_format(std::string format);
        NumberInputWidget& set_thumb_visible(bool visible);
        NumberInputWidget& set_thumb_size(float size);
        NumberInputWidget& set_thumb_color(ImColor color);

        template <typename T>
            requires std::constructible_from<NumberValue, T*>
        bool set_value(T value) {
            const bool changed = std::visit(
                [value](auto* bound_value) {
                    using BoundValue = std::remove_cv_t<std::remove_pointer_t<decltype(bound_value)>>;
                    if constexpr (!std::same_as<BoundValue, T>) {
                        return false;
                    } else if (*bound_value == value) {
                        return false;
                    } else {
                        *bound_value = value;
                        return true;
                    }
                },
                m_number
            );

            if (changed) {
                notify_change();
            }
            return changed;
        }

    private:
        bool paint() override;
        template <typename T>
        bool draw_value(T& value);

        void sync_value() const;
        void on_measure() override;

    protected:
        void apply_theme_defaults(const Theme& theme) override;

    private:
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
    };
} // namespace ui
