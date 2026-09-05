#pragma once

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

    using EasingFunction = float (*)(float);

    namespace easing {
        inline float linear(float progress) {
            return progress;
        }

        inline float in_quad(float progress) {
            return progress * progress;
        }

        inline float out_quad(float progress) {
            return progress * (2.0F - progress);
        }

        inline float in_out_quad(float progress) {
            return progress < 0.5F ? 2.0F * progress * progress : 1.0F - std::pow(-2.0F * progress + 2.0F, 2.0F) / 2.0F;
        }

        inline float in_cubic(float progress) {
            return progress * progress * progress;
        }

        inline float out_cubic(float progress) {
            return 1.0F - std::pow(1.0F - progress, 3.0F);
        }

        inline float in_out_cubic(float progress) {
            return progress < 0.5F ? 4.0F * progress * progress * progress
                                   : 1.0F - std::pow(-2.0F * progress + 2.0F, 3.0F) / 2.0F;
        }

        inline float in_sine(float progress) {
            return 1.0F - std::cos(progress * 1.57079632679F);
        }

        inline float out_sine(float progress) {
            return std::sin(progress * 1.57079632679F);
        }

        inline float in_out_sine(float progress) {
            return -(std::cos(3.14159265359F * progress) - 1.0F) / 2.0F;
        }

        inline float in_back(float progress) {
            constexpr float overshoot = 1.70158F;
            return (overshoot + 1.0F) * progress * progress * progress - overshoot * progress * progress;
        }

        inline float out_back(float progress) {
            constexpr float overshoot = 1.70158F;
            const float shifted = progress - 1.0F;
            return 1.0F + (overshoot + 1.0F) * shifted * shifted * shifted + overshoot * shifted * shifted;
        }

        inline float in_out_back(float progress) {
            constexpr float overshoot = 1.70158F * 1.525F;
            const float scaled = progress * 2.0F;
            if (scaled < 1.0F) {
                return scaled * scaled * ((overshoot + 1.0F) * scaled - overshoot) / 2.0F;
            }

            const float shifted = scaled - 2.0F;
            return (shifted * shifted * ((overshoot + 1.0F) * shifted + overshoot) + 2.0F) / 2.0F;
        }
    } // namespace easing

    struct TransitionSpec {
        float duration = 0.0F;
        EasingFunction easing = ui::easing::linear;
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

    template <typename T>
    struct Value {
        Value() = default;
        Value(T initial_value, float transition_duration = 0.0F)
            : value(std::move(initial_value)), duration(std::max(0.0F, transition_duration)) {}
        Value(T initial_value, TransitionSpec transition)
            : value(std::move(initial_value)), duration(std::max(0.0F, transition.duration)),
              easing(transition.easing != nullptr ? transition.easing : ui::easing::linear) {}

        T value{};
        float duration = 0.0F;
        EasingFunction easing = ui::easing::linear;

        void set(T new_value) {
            value = std::move(new_value);
            m_has_target = false;
        }

        void set_duration(float new_duration) {
            duration = std::max(0.0F, new_duration);
        }

        void set_transition(TransitionSpec transition) {
            duration = std::max(0.0F, transition.duration);
            easing = transition.easing != nullptr ? transition.easing : ui::easing::linear;
        }

    protected:
        // keeps settle checks from ending an easing curve before its configured duration.
        bool transition_complete() const {
            return !m_has_target || m_duration == 0.0F || m_elapsed >= m_duration;
        }

        float transition_progress(const Value& target, float dt) {
            const float target_duration = std::max(0.0F, target.duration);
            const EasingFunction target_easing = target.easing != nullptr ? target.easing : ui::easing::linear;
            const bool target_changed = !m_has_target || !transition_values_equal(m_target, target.value) ||
                                        m_duration != target_duration || m_easing != target_easing;
            if (target_changed) {
                m_start = value;
                m_target = target.value;
                m_duration = target_duration;
                m_easing = target_easing;
                m_elapsed = 0.0F;
                m_has_target = true;
            }

            if (m_duration == 0.0F) {
                return 1.0F;
            }

            m_elapsed = std::min(m_duration, m_elapsed + std::max(0.0F, dt));
            if (m_elapsed >= m_duration) {
                return 1.0F;
            }

            return m_easing(m_elapsed / m_duration);
        }

        const T& transition_start() const {
            return m_start;
        }

    private:
        T m_start{};
        T m_target{};
        float m_duration = 0.0F;
        EasingFunction m_easing = ui::easing::linear;
        float m_elapsed = 0.0F;
        bool m_has_target = false;
    };

    struct FloatValue : Value<float> {
        using Value::Value;

        void tick(const FloatValue& target, float dt) {
            const float progress = transition_progress(target, dt);
            if (transition_complete()) {
                value = target.value;
                return;
            }
            value = std::lerp(transition_start(), target.value, progress);
        }

        bool is_close(const FloatValue& target, float epsilon) const {
            return transition_complete() && std::fabs(value - target.value) <= epsilon;
        }
    };

    struct ColorValue : Value<ImColor> {
        using Value::Value;

        void tick(const ColorValue& target, float dt) {
            const float progress = transition_progress(target, dt);
            if (transition_complete()) {
                value = target.value;
                return;
            }
            const ImVec4& start = transition_start().Value;
            const ImVec4& end = target.value.Value;

            value.Value = {
                std::lerp(start.x, end.x, progress),
                std::lerp(start.y, end.y, progress),
                std::lerp(start.z, end.z, progress),
                std::lerp(start.w, end.w, progress),
            };
        }

        bool is_close(const ColorValue& target, float epsilon) const {
            const ImVec4& col = value.Value;
            const ImVec4& target_col = target.value.Value;

            return transition_complete() && std::fabs(col.x - target_col.x) <= epsilon &&
                   std::fabs(col.y - target_col.y) <= epsilon && std::fabs(col.z - target_col.z) <= epsilon &&
                   std::fabs(col.w - target_col.w) <= epsilon;
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

        void tick(const BoxShadowValue& target, float dt) {
            const float progress = transition_progress(target, dt);
            if (transition_complete()) {
                value = target.value;
                return;
            }
            const BoxShadow& start = transition_start();
            value.offset = {
                std::lerp(start.offset.x, target.value.offset.x, progress),
                std::lerp(start.offset.y, target.value.offset.y, progress),
            };
            value.blur = std::lerp(start.blur, target.value.blur, progress);
            value.spread = std::lerp(start.spread, target.value.spread, progress);
            value.color.Value = {
                std::lerp(start.color.Value.x, target.value.color.Value.x, progress),
                std::lerp(start.color.Value.y, target.value.color.Value.y, progress),
                std::lerp(start.color.Value.z, target.value.color.Value.z, progress),
                std::lerp(start.color.Value.w, target.value.color.Value.w, progress),
            };
        }

        bool is_close(const BoxShadowValue& target, float epsilon) const {
            return transition_complete() && std::fabs(value.offset.x - target.value.offset.x) <= epsilon &&
                   std::fabs(value.offset.y - target.value.offset.y) <= epsilon &&
                   std::fabs(value.blur - target.value.blur) <= epsilon &&
                   std::fabs(value.spread - target.value.spread) <= epsilon &&
                   std::fabs(value.color.Value.x - target.value.color.Value.x) <= epsilon &&
                   std::fabs(value.color.Value.y - target.value.color.Value.y) <= epsilon &&
                   std::fabs(value.color.Value.z - target.value.color.Value.z) <= epsilon &&
                   std::fabs(value.color.Value.w - target.value.color.Value.w) <= epsilon;
        }
    };

    struct Vec2Value : Value<ImVec2> {
        using Value::Value;

        void tick(const Vec2Value& target, float dt) {
            const float progress = transition_progress(target, dt);
            if (transition_complete()) {
                value = target.value;
                return;
            }
            const ImVec2& start = transition_start();
            value = {
                std::lerp(start.x, target.value.x, progress),
                std::lerp(start.y, target.value.y, progress),
            };
        }

        bool is_close(const Vec2Value& target, float epsilon) const {
            return transition_complete() && std::fabs(value.x - target.value.x) <= epsilon &&
                   std::fabs(value.y - target.value.y) <= epsilon;
        }
    };

    struct IntValue : Value<int> {
        using Value::Value;

        void tick(const IntValue& target, float dt) {
            const float progress = transition_progress(target, dt);
            if (transition_complete()) {
                value = target.value;
                return;
            }
            value = static_cast<int>(
                std::lround(std::lerp(static_cast<float>(transition_start()), static_cast<float>(target.value), progress))
            );
        }

        bool is_close(const IntValue& target, float epsilon) const {
            return transition_complete() && std::abs(value - target.value) <= epsilon;
        }
    };

    struct BoolValue : Value<bool> {
        using Value::Value;

        void tick(const BoolValue& target, float) {
            value = target.value;
        }

        bool is_close(const BoolValue&, float) const {
            return true;
        }
    };

    struct StringValue : Value<std::string> {
        using Value::Value;

        void tick(const StringValue& target, float) {
            value = target.value;
        }

        bool is_close(const StringValue&, float) const {
            return true;
        }
    };

    using StyleValue = std::variant<IntValue, FloatValue, StringValue, BoolValue, ColorValue, Vec2Value>;
} // namespace ui
