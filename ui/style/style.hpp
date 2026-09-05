#pragma once

#include "computed-style.hpp"

#include <algorithm>
#include <utility>

namespace ui {
    class StyledNode;
    class VisualState;

    enum class StyleType : int32_t {
        DEFAULT = 0,
        HOVER = 1,
        ACTIVE,
        FOCUS,
        _COUNT
    };

    class Style : public ComputedStyle {
        struct ValueEqual {
            template <typename T>
            bool operator()(const T& left, const T& right) const {
                return transition_values_equal(left, right);
            }
        };

        // compares, assigns, and notifies only when a plain property changes.
        template <typename Field>
        Style& set_property(Field ComputedStyle::* member, Field value) {
            Field& current = this->*member;
            if (ValueEqual{}(current, value)) {
                return *this;
            }

            current = std::move(value);
            notify_change();
            return *this;
        }

        template <typename Field, typename Normalize>
        Style& set_property(Field ComputedStyle::* member, Field value, Normalize normalize) {
            return set_property(member, normalize(std::move(value)));
        }

        // updates an animated property's target and optional duration without restarting an unchanged target.
        template <typename ValueType, typename Field>
        Style& set_animated_value(ValueType ComputedStyle::* member, Field value, float duration = -1.0F) {
            ValueType& current = this->*member;
            const bool changed = !ValueEqual{}(current.value, value);
            if (!changed && duration < 0.0F) {
                return *this;
            }

            if (changed) {
                current.set(std::move(value));
            }
            if (duration >= 0.0F) {
                current.set_duration(duration);
            }
            if (changed) {
                notify_change();
            }
            return *this;
        }

        // updates an animated property's target and easing metadata in one operation.
        template <typename ValueType, typename Field>
        Style& set_animated_transition(ValueType ComputedStyle::* member, Field value, TransitionSpec transition) {
            ValueType& current = this->*member;
            const bool changed = !ValueEqual{}(current.value, value);
            if (changed) {
                current.set(std::move(value));
            }
            current.set_transition(transition);
            if (changed) {
                notify_change();
            }
            return *this;
        }

        static BoxShadow normalize_box_shadow(BoxShadow value) {
            value.blur = std::max(0.0F, value.blur);
            value.color.Value.w = std::clamp(value.color.Value.w, 0.0F, 1.0F);
            return value;
        }

        static ImVec2 normalize_padding(ImVec2 value) {
            return {std::max(0.0F, value.x), std::max(0.0F, value.y)};
        }

    public:
        using ChangeCallback = void (*)(void*);
        using PushState = ComputedStyle::PushState;

        using ComputedStyle::alpha;
        using ComputedStyle::background_color;
        using ComputedStyle::blur;
        using ComputedStyle::border;
        using ComputedStyle::border_color;
        using ComputedStyle::border_radius;
        using ComputedStyle::border_style;
        using ComputedStyle::border_thickness;
        using ComputedStyle::box_shadow;
        using ComputedStyle::color;
        using ComputedStyle::cursor;
        using ComputedStyle::font;
        using ComputedStyle::line_height;
        using ComputedStyle::padding;
        using ComputedStyle::use_background_for_scrollbar;
        using ComputedStyle::variables;

        Style() = default;

        const ComputedStyle& computed_style() const {
            return *this;
        }

        Style& font(ImFont* value) {
            return set_property(&ComputedStyle::m_font, value);
        }

        ColorValue& color() {
            return m_color;
        }

        ColorValue& border_color() {
            return m_border_color;
        }

        ColorValue& background_color() {
            return m_background_color;
        }

        StyleVariableStore& variables() {
            return m_vars;
        }

        /// clamps both padding axes, updates the target, and optionally changes its duration.
        Style& padding(ImVec2 value, float transition_duration = -1.0F) {
            return set_animated_value(&ComputedStyle::m_padding, normalize_padding(value), transition_duration);
        }

        /// clamps both padding axes, updates the target, and stores its easing metadata.
        Style& padding(ImVec2 value, TransitionSpec transition) {
            return set_animated_transition(&ComputedStyle::m_padding, normalize_padding(value), transition);
        }

        Style& control(const Theme& theme, ImVec2 padding = {10.0F, 6.0F}) {
            return color(theme.text_color)
                .background_color(theme.control_background_color, 0.15F)
                .border_color(theme.control_border_color, 0.15F)
                .padding(padding)
                .border(BORDER_ALL)
                .border_radius(theme.control_rounding)
                .border_thickness(theme.control_border_thickness);
        }

        Style& line_height(float value, float transition_duration = -1.0F) {
            return set_animated_value(&ComputedStyle::m_line_height, std::max(0.0F, value), transition_duration);
        }

        Style& line_height(float value, TransitionSpec transition) {
            return set_animated_transition(&ComputedStyle::m_line_height, std::max(0.0F, value), transition);
        }

        Style& alpha(float value) {
            return set_property(&ComputedStyle::m_alpha, value, [](float resolved) { return std::clamp(resolved, 0.0F, 1.0F); });
        }

        Style& cursor(ImGuiMouseCursor value) {
            return set_property(&ComputedStyle::m_cursor, value);
        }

        Style& use_background_for_scrollbar(bool value = true) {
            return set_property(&ComputedStyle::m_use_background_for_scrollbar, value);
        }

        Style& color(ImColor value, float transition_duration = -1.0F) {
            return set_animated_value(&ComputedStyle::m_color, std::move(value), transition_duration);
        }

        Style& color(ImColor value, TransitionSpec transition) {
            return set_animated_transition(&ComputedStyle::m_color, std::move(value), transition);
        }

        Style& border_color(ImColor value, float transition_duration = -1.0F) {
            return set_animated_value(&ComputedStyle::m_border_color, std::move(value), transition_duration);
        }

        Style& border_color(ImColor value, TransitionSpec transition) {
            return set_animated_transition(&ComputedStyle::m_border_color, std::move(value), transition);
        }

        Style& background_color(ImColor value, float transition_duration = -1.0F) {
            return set_animated_value(&ComputedStyle::m_background_color, std::move(value), transition_duration);
        }

        Style& background_color(ImColor value, TransitionSpec transition) {
            return set_animated_transition(&ComputedStyle::m_background_color, std::move(value), transition);
        }

        Style& box_shadow(BoxShadow value, float transition_duration = -1.0F) {
            return set_animated_value(&ComputedStyle::m_box_shadow, normalize_box_shadow(std::move(value)), transition_duration);
        }

        Style& box_shadow(BoxShadow value, TransitionSpec transition) {
            return set_animated_transition(&ComputedStyle::m_box_shadow, normalize_box_shadow(std::move(value)), transition);
        }

        Style& blur(int value) {
            return set_property(&ComputedStyle::m_blur, value, [](int resolved) { return std::max(0, resolved); });
        }

        Style& border_radius(float value) {
            return set_property(&ComputedStyle::m_border_radius, value, [](float resolved) { return std::max(0.0F, resolved); });
        }

        Style& border_thickness(float value) {
            return set_property(&ComputedStyle::m_border_thickness, value, [](float resolved) {
                return std::max(0.0F, resolved);
            });
        }

        Style& border(uint8_t value) {
            return set_property(&ComputedStyle::m_border, value, [](uint8_t resolved) {
                return static_cast<uint8_t>(resolved & BORDER_ALL);
            });
        }

        Style& border_style(BorderStyle value) {
            return set_property(&ComputedStyle::m_border_style, value);
        }

        static void lerp(Style& style, const Style& target, float dt);

    private:
        friend class StyledNode;
        friend class VisualState;
        friend class PaintSlot;

        void set_change_callback(void* owner, ChangeCallback callback) {
            m_change_owner = owner;
            m_change_callback = callback;
        }

        void notify_change() const {
            if (m_change_callback != nullptr) {
                m_change_callback(m_change_owner);
            }
        }

        void* m_change_owner = nullptr;
        ChangeCallback m_change_callback = nullptr;
    };

} // namespace ui
