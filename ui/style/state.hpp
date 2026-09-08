#pragma once

#include "animation.hpp"
#include "style.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

namespace ui {
    static constexpr float OPACITY_TRANSITION_DURATION = 0.15F;
    static constexpr float VISIBILITY_OPACITY_THRESHOLD = 0.002f;

    class VisualState {
    public:
        VisualState() {
            current_opacity.value = m_opacity;
            snap_to_style(StyleType::DEFAULT);
        }

        void set_change_callback(void* owner, Style::ChangeCallback callback) {
            for (Style& style : styles) {
                style.set_change_callback(owner, callback);
            }
            if (m_transition_style.has_value()) {
                m_transition_style->set_change_callback(owner, callback);
            }
        }

        /// selects a slot without running a style transition.
        void snap_to_style(StyleType type) {
            m_target_style = type;
            m_transition_style.reset();
        }

        bool is_visible() const {
            return visible &&
                   (m_opacity >= VISIBILITY_OPACITY_THRESHOLD || current_opacity.value >= VISIBILITY_OPACITY_THRESHOLD);
        }

        void set_visible(bool value) {
            visible = value;
        }

        void set_opacity(float value) {
            set_opacity(value, {OPACITY_TRANSITION_DURATION, ui::easing::linear});
        }

        void set_opacity(float value, TransitionSpec transition) {
            m_opacity_transition = transition;
            m_opacity = std::clamp(value, 0.0f, 1.0f);
        }

        void fade_in() {
            fade_in({OPACITY_TRANSITION_DURATION, ui::easing::linear});
        }

        void fade_in(TransitionSpec transition) {
            visible = true;
            if (first_frame) current_opacity.value = 0.0F;
            set_opacity(1.0f, transition);
        }

        void fade_out() {
            fade_out({OPACITY_TRANSITION_DURATION, ui::easing::linear});
        }

        void fade_out(TransitionSpec transition) {
            set_opacity(0.0f, transition);
        }

        bool accepts_input() const {
            return visible && m_opacity >= VISIBILITY_OPACITY_THRESHOLD;
        }

        float opacity() const {
            if (first_frame) {
                return 0.0f;
            }

            return current_opacity.value;
        }

        bool transitioning() const {
            return current_opacity.value != m_opacity || m_transition_style.has_value() || !m_animation_steps.empty() ||
                   !m_animation_tracks.empty() || !m_animation_callbacks.empty();
        }

        void update(float dt) {
            const FloatValue target_opacity{m_opacity, m_opacity_transition};
            current_opacity.tick(target_opacity, dt);
            if (current_opacity.is_transition_complete()) {
                current_opacity.value = m_opacity;
            }

            if (m_transition_style.has_value()) {
                const Style& target_style = styles[static_cast<size_t>(m_target_style)];
                if (!Style::lerp(*m_transition_style, target_style, dt)) {
                    m_transition_style.reset();
                }
            }

            first_frame = false;
            update_animations(dt);
        }

        /// selects a style slot and begins interpolation when necessary.
        void set_style(StyleType type) {
            if (m_target_style == type) {
                return;
            }

            if (!m_transition_style.has_value()) {
                m_transition_style.emplace(styles[static_cast<size_t>(m_target_style)]);
            }

            m_target_style = type;
        }

        /// interaction precedence is active, focus, hover, then default.
        void set_item_state(bool hovered, bool active, bool focused = false) {
            if (active) {
                set_style(StyleType::ACTIVE);
                return;
            }

            if (focused) {
                set_style(StyleType::FOCUS);
                return;
            }

            set_style(hovered ? StyleType::HOVER : StyleType::DEFAULT);
        }

        template <typename Func>
        VisualState& configure_all_styles(Func&& func) {
            for (auto& style : styles) {
                func(style);
            }

            return *this;
        }

        template <typename Func>
        VisualState& configure_style(StyleType type, Func&& func) {
            func(styles[static_cast<size_t>(type)]);
            return *this;
        }

        StyleType style_type() const {
            return m_target_style;
        }

        AnimationSequence animate();

        void cancel_animations();

        /// resolved style currently used for drawing.
        Style& style() {
            return m_transition_style.has_value() ? *m_transition_style : styles[static_cast<size_t>(m_target_style)];
        }

        /// mutable named slot, independent from the current transition.
        Style& style(StyleType type) {
            return styles[static_cast<size_t>(type)];
        }

        const Style& style() const {
            return m_transition_style.has_value() ? *m_transition_style : styles[static_cast<size_t>(m_target_style)];
        }

        const ComputedStyle& computed_style() const {
            return m_has_presentation_style ? m_presentation_style : style();
        }

        const Style& style(StyleType type) const {
            return styles[static_cast<size_t>(type)];
        }

    private:
        friend class AnimationSequence;

        struct AnimationStep {
            AnimationProperty property;
            std::optional<AnimationValue> value;
            TransitionSpec transition;
            float start = 0.0F;
        };

        struct AnimationTrack {
            AnimationProperty property;
            AnimationValue start;
            AnimationValue target;
            TransitionSpec transition;
            float started_at = 0.0F;
            bool release = false;
        };

        struct AnimationCallback {
            float at = 0.0F;
            std::function<void()> callback;
        };

        void schedule_animation(
            AnimationProperty property, std::optional<AnimationValue> value, float start, TransitionSpec transition
        );
        void schedule_animation_callback(float at, std::function<void()> callback);
        void update_animations(float dt);
        void apply_animation_value(Style& style, AnimationProperty property, const AnimationValue& value) const;
        AnimationValue track_value(const AnimationTrack& track) const;
        AnimationValue animation_value(const ComputedStyle& style, AnimationProperty property) const;
        bool has_animation_overrides() const;
        bool has_layout_override() const;
        static bool affects_layout(AnimationProperty property);

        StyleType m_target_style = StyleType::DEFAULT;
        FloatValue current_opacity;
        Style styles[static_cast<size_t>(StyleType::COUNT)];
        std::optional<Style> m_transition_style;
        Style m_presentation_style;
        std::array<std::optional<AnimationValue>, static_cast<size_t>(AnimationProperty::COUNT)> m_animation_overrides;
        std::vector<AnimationStep> m_animation_steps;
        std::vector<AnimationTrack> m_animation_tracks;
        std::vector<AnimationCallback> m_animation_callbacks;
        float m_animation_time = 0.0F;
        float m_opacity = 1.0f;
        TransitionSpec m_opacity_transition{OPACITY_TRANSITION_DURATION, ui::easing::linear};
        bool visible = true;
        bool first_frame = true;
        bool m_has_presentation_style = false;
    };

} // namespace ui
