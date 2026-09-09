#pragma once

#include "animation.hpp"
#include "style.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <span>

namespace ui {
    static constexpr float OPACITY_TRANSITION_DURATION = 0.15F;
    static constexpr float VISIBILITY_OPACITY_THRESHOLD = 0.002f;

    enum class StyleAnimationProperty : uint8_t {
        PaddingX,
        PaddingY,
        MarginX,
        MarginY,
        Rotation,
        Scale,
        Color,
        BorderColor,
        BackgroundColor,
        Count,
    };

    struct StyleAnimationSlot {
        /// selects the style property read and written by this slot.
        StyleAnimationProperty property = StyleAnimationProperty::PaddingX;
        /// holds a value while an animation overrides the configured style.
        std::optional<AnimationValue> override;
        /// stores the value visible before the current animation frame.
        AnimationValue current = 0.0F;
        /// stores the configured value restored by a release track.
        AnimationValue base = 0.0F;
        /// marks layout invalid when the property changes padding or margin.
        bool affects_layout = false;
        bool* layout_dirty = nullptr;
    };

    class VisualState {
    public:
        VisualState();

        void set_change_callback(void* owner, Style::ChangeCallback callback) {
            m_change_owner = owner;
            m_change_callback = callback;

            for (Style& style : styles) {
                style.set_change_callback(owner, callback);
            }

            if (m_transition_style.has_value()) {
                m_transition_style->set_change_callback(owner, callback);
            }
        }

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
            set_opacity(value, {OPACITY_TRANSITION_DURATION, easing::linear});
        }

        void set_opacity(float value, TransitionSpec transition) {
            m_opacity_transition = transition;
            m_opacity = std::clamp(value, 0.0f, 1.0f);
        }

        void fade_in() {
            fade_in({OPACITY_TRANSITION_DURATION, easing::linear});
        }

        void fade_in(TransitionSpec transition) {
            visible = true;
            if (first_frame) current_opacity.value = 0.0F;
            set_opacity(1.0f, transition);
        }

        void fade_out() {
            fade_out({OPACITY_TRANSITION_DURATION, easing::linear});
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
            return current_opacity.value != m_opacity || m_transition_style.has_value() || m_style_animator.transitioning() ||
                   m_animator.transitioning();
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

        void set_style(StyleType type) {
            if (m_target_style == type) {
                return;
            }

            if (!m_transition_style.has_value()) {
                m_transition_style.emplace(styles[static_cast<size_t>(m_target_style)]);
            }

            m_target_style = type;
        }

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

        StyleAnimationSequence animate();

        Animator& animator() {
            return m_animator;
        }

        const Animator& animator() const {
            return m_animator;
        }

        void cancel_animations();

        Style& style() {
            return m_transition_style.has_value() ? *m_transition_style : styles[static_cast<size_t>(m_target_style)];
        }

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
        friend class StyleAnimationSequence;

        void update_animations(float dt);
        std::span<StyleAnimationSlot> animation_slots() {
            return m_animation_slots;
        }

        std::span<const StyleAnimationSlot> animation_slots() const {
            return m_animation_slots;
        }

        StyleAnimationSlot& slot(StyleAnimationProperty property) {
            return m_animation_slots[static_cast<size_t>(property)];
        }

        AnimationTarget target(StyleAnimationSlot& slot);
        bool has_animation_overrides() const;

        StyleType m_target_style = StyleType::DEFAULT;
        FloatValue current_opacity;
        Style styles[static_cast<size_t>(StyleType::COUNT)];
        std::optional<Style> m_transition_style;
        Style m_presentation_style;
        std::array<StyleAnimationSlot, static_cast<size_t>(StyleAnimationProperty::Count)> m_animation_slots;
        Animator m_style_animator;
        Animator m_animator;
        float m_opacity = 1.0f;
        TransitionSpec m_opacity_transition{OPACITY_TRANSITION_DURATION, easing::linear};
        bool visible = true;
        bool first_frame = true;
        bool m_has_presentation_style = false;
        bool m_layout_dirty = false;
        void* m_change_owner = nullptr;
        Style::ChangeCallback m_change_callback = nullptr;
    };

} // namespace ui
