#pragma once

#include "tween/animator.hpp"

#include <cstdint>
#include <functional>

namespace ui {
    class VisualState;
    struct StyleAnimationSlot;
    enum class StyleAnimationProperty : uint8_t;

    class StyleAnimationSequence final {
    public:
        explicit StyleAnimationSequence(VisualState& state, AnimationSequence sequence) : m_state(state), m_sequence(sequence) {}

        StyleAnimationSequence& to(StyleAnimationProperty property, AnimationValue value, TransitionSpec transition = {});
        StyleAnimationSequence& by(StyleAnimationProperty property, AnimationValue value, TransitionSpec transition = {});
        /// animates a property back to its configured style value, then removes the override.
        StyleAnimationSequence& release(StyleAnimationProperty property, TransitionSpec transition = {});
        StyleAnimationSequence& release_all(TransitionSpec transition = {});

        StyleAnimationSequence& then(float delay = 0.0F);
        StyleAnimationSequence& delay(float duration);
        StyleAnimationSequence& end(std::function<void()> callback);

    private:
        VisualState& m_state;
        AnimationSequence m_sequence;
    };
} // namespace ui
