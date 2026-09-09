#pragma once

#include "../transition.hpp"

#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <string>
#include <utility>
#include <variant>

namespace ui {
    struct BoxShadow {
        ImVec2 offset{};
        float blur = 0.0F;
        float spread = 0.0F;
        ImColor color = ImColor{0.0F, 0.0F, 0.0F, 0.0F};
    };

    template <typename T>
    bool transition_values_equal(const T& left, const T& right) {
        return left == right;
    }

    inline bool transition_values_equal(const ImVec2& left, const ImVec2& right) {
        return left.x == right.x && left.y == right.y;
    }

    inline bool transition_values_equal(const ImColor& left, const ImColor& right) {
        return left.Value.x == right.Value.x && left.Value.y == right.Value.y && left.Value.z == right.Value.z &&
               left.Value.w == right.Value.w;
    }

    inline bool transition_values_equal(const BoxShadow& left, const BoxShadow& right) {
        return transition_values_equal(left.offset, right.offset) && left.blur == right.blur && left.spread == right.spread &&
               transition_values_equal(left.color, right.color);
    }

    inline ImColor with_alpha(ImColor color, float alpha) {
        color.Value.w = std::clamp(alpha, 0.0F, 1.0F);
        return color;
    }

    /// captures eased progress and whether ticking advanced or retargeted the property.
    struct TransitionStep {
        float progress = 1.0F;
        bool changed = false;
    };

    template <typename T>
    struct Value {
        Value() = default;
        Value(T initial_value, float transition_duration = 0.0F)
            : value(std::move(initial_value)), duration(std::max(0.0F, transition_duration)) {}
        Value(T initial_value, TransitionSpec transition)
            : value(std::move(initial_value)), duration(std::max(0.0F, transition.duration)),
              easing(transition.easing != nullptr ? transition.easing : easing::linear) {}

        T value{};
        float duration = 0.0F;
        EasingFunction easing = easing::linear;

        void set(T new_value) {
            value = std::move(new_value);
            m_has_target = false;
        }

        void set_duration(float new_duration) {
            duration = std::max(0.0F, new_duration);
        }

        void set_transition(TransitionSpec transition) {
            duration = std::max(0.0F, transition.duration);
            easing = transition.easing != nullptr ? transition.easing : easing::linear;
        }

        bool is_transitioning() const {
            return m_has_target && m_elapsed < m_duration;
        }

        bool is_transition_complete() const {
            return !is_transitioning();
        }

    protected:
        TransitionStep transition_progress(const Value& target, float dt) {
            const float target_duration = std::max(0.0F, target.duration);
            const EasingFunction target_easing = target.easing != nullptr ? target.easing : easing::linear;
            const bool target_changed = !m_has_target || !transition_values_equal(m_target, target.value) ||
                                        m_duration != target_duration || m_easing != target_easing;
            const float previous_elapsed = m_elapsed;
            if (target_changed) {
                m_start = value;
                m_target = target.value;
                m_duration = target_duration;
                m_easing = target_easing;
                m_elapsed = 0.0F;
                m_has_target = true;
            }

            if (m_duration == 0.0F) {
                return {1.0F, target_changed};
            }

            m_elapsed = std::min(m_duration, m_elapsed + std::max(0.0F, dt));
            if (m_elapsed >= m_duration) {
                return {1.0F, target_changed || m_elapsed != previous_elapsed};
            }

            return {m_easing(m_elapsed / m_duration), target_changed || m_elapsed != previous_elapsed};
        }

        const T& transition_start() const {
            return m_start;
        }

    private:
        T m_start{};
        T m_target{};
        float m_duration = 0.0F;
        EasingFunction m_easing = easing::linear;
        float m_elapsed = 0.0F;
        bool m_has_target = false;
    };

    struct FloatValue : Value<float> {
        using Value::Value;

        bool tick(const FloatValue& target, float dt) {
            const TransitionStep step = transition_progress(target, dt);
            if (is_transition_complete()) {
                value = target.value;
                return step.changed;
            }
            value = std::lerp(transition_start(), target.value, step.progress);
            return step.changed;
        }
    };

    struct ColorValue : Value<ImColor> {
        using Value::Value;

        bool tick(const ColorValue& target, float dt) {
            const TransitionStep step = transition_progress(target, dt);

            if (is_transition_complete()) {
                value = target.value;
                return step.changed;
            }

            const ImVec4& start = transition_start().Value;
            const ImVec4& end = target.value.Value;

            value.Value = {
                std::lerp(start.x, end.x, step.progress),
                std::lerp(start.y, end.y, step.progress),
                std::lerp(start.z, end.z, step.progress),
                std::lerp(start.w, end.w, step.progress),
            };
            return step.changed;
        }

        ImVec4 get() const {
            return value.Value;
        }

        ImU32 get_col() const {
            return ImGui::GetColorU32(value.Value);
        }
    };

    struct BoxShadowValue : Value<BoxShadow> {
        using Value::Value;

        bool tick(const BoxShadowValue& target, float dt) {
            const TransitionStep step = transition_progress(target, dt);
            if (is_transition_complete()) {
                value = target.value;
                return step.changed;
            }
            const BoxShadow& start = transition_start();
            value.offset = {
                std::lerp(start.offset.x, target.value.offset.x, step.progress),
                std::lerp(start.offset.y, target.value.offset.y, step.progress),
            };
            value.blur = std::lerp(start.blur, target.value.blur, step.progress);
            value.spread = std::lerp(start.spread, target.value.spread, step.progress);
            value.color.Value = {
                std::lerp(start.color.Value.x, target.value.color.Value.x, step.progress),
                std::lerp(start.color.Value.y, target.value.color.Value.y, step.progress),
                std::lerp(start.color.Value.z, target.value.color.Value.z, step.progress),
                std::lerp(start.color.Value.w, target.value.color.Value.w, step.progress),
            };
            return step.changed;
        }
    };

    struct Vec2Value : Value<ImVec2> {
        using Value::Value;

        bool tick(const Vec2Value& target, float dt) {
            const TransitionStep step = transition_progress(target, dt);
            if (is_transition_complete()) {
                value = target.value;
                return step.changed;
            }
            const ImVec2& start = transition_start();
            value = {
                std::lerp(start.x, target.value.x, step.progress),
                std::lerp(start.y, target.value.y, step.progress),
            };
            return step.changed;
        }
    };

    struct IntValue : Value<int> {
        using Value::Value;

        bool tick(const IntValue& target, float dt) {
            const TransitionStep step = transition_progress(target, dt);
            if (is_transition_complete()) {
                value = target.value;
                return step.changed;
            }
            value = static_cast<int>(
                std::lround(std::lerp(static_cast<float>(transition_start()), static_cast<float>(target.value), step.progress))
            );
            return step.changed;
        }
    };

    struct BoolValue : Value<bool> {
        using Value::Value;

        bool tick(const BoolValue& target, float) {
            const bool changed = value != target.value;
            value = target.value;
            return changed;
        }
    };

    struct StringValue : Value<std::string> {
        using Value::Value;

        bool tick(const StringValue& target, float) {
            const bool changed = value != target.value;
            value = target.value;
            return changed;
        }
    };

    using StyleValue = std::variant<IntValue, FloatValue, StringValue, BoolValue, ColorValue, Vec2Value>;
} // namespace ui
