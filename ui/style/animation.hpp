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

        StyleAnimationSequence& to(StyleAnimationProperty property, AnimationValue value, TransitionSpec transition = {});
        StyleAnimationSequence& by(StyleAnimationProperty property, AnimationValue value, TransitionSpec transition = {});
        StyleAnimationSequence& release(StyleAnimationProperty property, TransitionSpec transition = {});
        StyleAnimationSequence& release_all(TransitionSpec transition = {});

        StyleAnimationSequence& then(float delay = 0.0F);
        StyleAnimationSequence& delay(float duration);
        StyleAnimationSequence& end(std::function<void()> callback);

    private:
        friend class VisualState;

        VisualState& m_state;
        AnimationSequence m_sequence;
    };
} // namespace ui
