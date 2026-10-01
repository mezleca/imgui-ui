#pragma once

#include "animation.hpp"
#include "style.hpp"

#include <array>
#include <optional>

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
        /// holds a value while an animation overrides the configured style.
        std::optional<AnimationValue> override;
        /// stores the value visible before the current animation frame.
        AnimationValue current = 0.0F;
        /// stores the configured value restored by a release track.
        AnimationValue base = 0.0F;
        /// points to the owner's invalidation flag only for properties that change padding or margin.
        bool* layout_dirty = nullptr;
    };

    /// tracks animated style property overrides layered on top of a StyledNode's active style.
    class VisualState {
    public:
        VisualState();
        void set_change_callback(void* owner, Style::ChangeCallback callback);

        Style& style(StyleType type = StyleType::DEFAULT) {
            return styles[static_cast<size_t>(type)];
        }

        const Style& style(StyleType type = StyleType::DEFAULT) const {
            return styles[static_cast<size_t>(type)];
        }

        const ComputedStyle& computed_style() const {
            return m_has_presentation_style ? m_presentation_style : active_style();
        }

        template <typename Func>
        VisualState& configure_all_styles(Func&& func) {
            for (auto& style : styles) {
                func(style);
            }

            return *this;
        }

        void set_style(StyleType type);
        void snap_to_style(StyleType type);
        void set_item_state(bool hovered, bool active, bool focused = false);
        StyleType style_type() const {
            return m_target_style;
        }

        void set_visible(bool value) {
            visible = value;
        }

        bool is_visible() const {
            return visible &&
                   (m_opacity >= VISIBILITY_OPACITY_THRESHOLD || current_opacity.value >= VISIBILITY_OPACITY_THRESHOLD);
        }

        bool accepts_input() const {
            return visible && m_opacity >= VISIBILITY_OPACITY_THRESHOLD;
        }

        void set_opacity(float value, TransitionSpec transition = {OPACITY_TRANSITION_DURATION, easing::linear});
        void fade_in(TransitionSpec transition = {OPACITY_TRANSITION_DURATION, easing::linear});
        void fade_out(TransitionSpec transition = {OPACITY_TRANSITION_DURATION, easing::linear}) {
            set_opacity(0.0F, transition);
        }

        float opacity() const {
            return first_frame ? 0.0F : current_opacity.value;
        }

        StyleAnimationSequence animate();
        void cancel_animations();
        Animator& animator() {
            return m_animator;
        }

        const Animator& animator() const {
            return m_animator;
        }

        void update(float dt);
        bool transitioning() const;

    private:
        friend class StyleAnimationSequence;

        const Style& active_style() const {
            return m_transition_style.has_value() ? *m_transition_style : style(m_target_style);
        }

        void update_animations(float dt);
        StyleAnimationSlot& slot(StyleAnimationProperty property) {
            return m_animation_slots[static_cast<size_t>(property)];
        }

        static AnimationTarget target(StyleAnimationSlot& slot);
        bool has_animation_overrides() const;

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
        StyleType m_target_style = StyleType::DEFAULT;
        void* m_change_owner = nullptr;
        Style::ChangeCallback m_change_callback = nullptr;
    };

} // namespace ui
