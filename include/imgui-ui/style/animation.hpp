#pragma once

#include "tween/animator.hpp"

#include <cstdint>
#include <functional>

namespace ui {
    class VisualState;
    struct StyleAnimationSlot;
    enum class StyleAnimationProperty : uint8_t;

    /// schedules tracks for the visual properties owned by one styled node.
    class StyleAnimationSequence final {
    public:
        explicit StyleAnimationSequence(VisualState& state, AnimationSequence sequence) : m_state(state), m_sequence(sequence) {}

        /// animates one property from its current displayed value to value.
        StyleAnimationSequence& to(StyleAnimationProperty property, AnimationValue value, TransitionSpec transition = {});
        /// animates one property by a relative amount from its current displayed value.
        StyleAnimationSequence& by(StyleAnimationProperty property, AnimationValue value, TransitionSpec transition = {});
        /// animates a property back to its configured style value, then removes the override.
        StyleAnimationSequence& release(StyleAnimationProperty property, TransitionSpec transition = {});
        /// returns all overridden properties to their configured style values.
        StyleAnimationSequence& release_all(TransitionSpec transition = {});

        StyleAnimationSequence& then(float delay = 0.0F);
        StyleAnimationSequence& delay(float duration);
        StyleAnimationSequence& end(std::function<void()> callback);

    private:
        VisualState& m_state;
        AnimationSequence m_sequence;
    };
} // namespace ui
