#pragma once

#include "../animation.hpp"

#include <functional>

namespace ui {
    class VisualState;
    struct StyleAnimationSlot;

    /// schedules tracks for the visual properties owned by one styled node.
    class StyleAnimationSequence final {
    public:
        explicit StyleAnimationSequence(VisualState& state, AnimationSequence sequence)
            : m_state(state), m_sequence(std::move(sequence)) {}

        StyleAnimationSequence& padding_x(float value, TransitionSpec transition = {});
        StyleAnimationSequence& padding_y(float value, TransitionSpec transition = {});
        StyleAnimationSequence& margin_x(float value, TransitionSpec transition = {});
        StyleAnimationSequence& margin_y(float value, TransitionSpec transition = {});
        StyleAnimationSequence& rotation(float value, TransitionSpec transition = {});
        StyleAnimationSequence& scale(ImVec2 value, TransitionSpec transition = {});
        StyleAnimationSequence& scale(float value, TransitionSpec transition = {});
        StyleAnimationSequence& color(ImColor value, TransitionSpec transition = {});
        StyleAnimationSequence& border_color(ImColor value, TransitionSpec transition = {});
        StyleAnimationSequence& background_color(ImColor value, TransitionSpec transition = {});

        StyleAnimationSequence& padding_x_by(float value, TransitionSpec transition = {});
        StyleAnimationSequence& padding_y_by(float value, TransitionSpec transition = {});
        StyleAnimationSequence& margin_x_by(float value, TransitionSpec transition = {});
        StyleAnimationSequence& margin_y_by(float value, TransitionSpec transition = {});
        StyleAnimationSequence& rotation_by(float degrees, TransitionSpec transition = {});
        StyleAnimationSequence& scale_by(ImVec2 factor, TransitionSpec transition = {});
        StyleAnimationSequence& scale_by(float factor, TransitionSpec transition = {});
        StyleAnimationSequence& color_by(ImVec4 value, TransitionSpec transition = {});
        StyleAnimationSequence& border_color_by(ImVec4 value, TransitionSpec transition = {});
        StyleAnimationSequence& background_color_by(ImVec4 value, TransitionSpec transition = {});

        StyleAnimationSequence& release_padding_x(TransitionSpec transition = {});
        StyleAnimationSequence& release_padding_y(TransitionSpec transition = {});
        StyleAnimationSequence& release_margin_x(TransitionSpec transition = {});
        StyleAnimationSequence& release_margin_y(TransitionSpec transition = {});
        StyleAnimationSequence& release_rotation(TransitionSpec transition = {});
        StyleAnimationSequence& release_scale(TransitionSpec transition = {});
        StyleAnimationSequence& release_color(TransitionSpec transition = {});
        StyleAnimationSequence& release_border_color(TransitionSpec transition = {});
        StyleAnimationSequence& release_background_color(TransitionSpec transition = {});
        StyleAnimationSequence& release_all(TransitionSpec transition = {});

        StyleAnimationSequence& then(float delay = 0.0F);
        StyleAnimationSequence& delay(float duration);
        StyleAnimationSequence& end(std::function<void()> callback);

    private:
        friend class VisualState;

        StyleAnimationSequence& add(StyleAnimationSlot& slot, AnimationValue value, TransitionSpec transition);
        StyleAnimationSequence& add_by(StyleAnimationSlot& slot, AnimationValue value, TransitionSpec transition);
        StyleAnimationSequence& release(StyleAnimationSlot& slot, TransitionSpec transition);
        float current_float(StyleAnimationSlot& slot) const;
        ImVec2 current_vec2(StyleAnimationSlot& slot) const;

        VisualState& m_state;
        AnimationSequence m_sequence;
    };
} // namespace ui
