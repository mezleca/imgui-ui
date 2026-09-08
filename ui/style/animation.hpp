#pragma once

#include "values.hpp"

#include <cstdint>
#include <optional>
#include <variant>

namespace ui {
    class VisualState;

    enum class AnimationProperty : uint8_t {
        PaddingX,
        PaddingY,
        MarginX,
        MarginY,
        Color,
        BorderColor,
        BackgroundColor,
        _COUNT,
    };

    using AnimationValue = std::variant<float, ImColor>;

    class AnimationSequence final {
    public:
        AnimationSequence& padding_x(float value, TransitionSpec transition = {});
        AnimationSequence& padding_y(float value, TransitionSpec transition = {});
        AnimationSequence& margin_x(float value, TransitionSpec transition = {});
        AnimationSequence& margin_y(float value, TransitionSpec transition = {});
        AnimationSequence& color(ImColor value, TransitionSpec transition = {});
        AnimationSequence& border_color(ImColor value, TransitionSpec transition = {});
        AnimationSequence& background_color(ImColor value, TransitionSpec transition = {});

        AnimationSequence& release_padding_x(TransitionSpec transition = {});
        AnimationSequence& release_padding_y(TransitionSpec transition = {});
        AnimationSequence& release_margin_x(TransitionSpec transition = {});
        AnimationSequence& release_margin_y(TransitionSpec transition = {});
        AnimationSequence& release_color(TransitionSpec transition = {});
        AnimationSequence& release_border_color(TransitionSpec transition = {});
        AnimationSequence& release_background_color(TransitionSpec transition = {});

        AnimationSequence& then(float delay = 0.0F);
        AnimationSequence& delay(float duration);

    private:
        friend class VisualState;

        explicit AnimationSequence(VisualState& state) : m_state(state) {}

        AnimationSequence& add(AnimationProperty property, std::optional<AnimationValue> value, TransitionSpec transition);

        VisualState& m_state;
        float m_cursor = 0.0F;
        float m_end = 0.0F;
    };
} // namespace ui
