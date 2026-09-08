#pragma once

#include "values.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <variant>

namespace ui {
    class VisualState;

    enum class AnimationProperty : uint8_t {
        PaddingX,
        PaddingY,
        MarginX,
        MarginY,
        Rotation,
        Scale,
        Color,
        BorderColor,
        BackgroundColor,
        COUNT,
    };

    using AnimationValue = std::variant<float, ImVec2, ImColor>;

    class AnimationSequence final {
    public:
        AnimationSequence& padding_x(float value, TransitionSpec transition = {});
        AnimationSequence& padding_y(float value, TransitionSpec transition = {});
        AnimationSequence& margin_x(float value, TransitionSpec transition = {});
        AnimationSequence& margin_y(float value, TransitionSpec transition = {});
        AnimationSequence& rotation(float value, TransitionSpec transition = {});
        AnimationSequence& scale(ImVec2 value, TransitionSpec transition = {});
        AnimationSequence& scale(float value, TransitionSpec transition = {});
        AnimationSequence& color(ImColor value, TransitionSpec transition = {});
        AnimationSequence& border_color(ImColor value, TransitionSpec transition = {});
        AnimationSequence& background_color(ImColor value, TransitionSpec transition = {});

        AnimationSequence& padding_x_by(float value, TransitionSpec transition = {});
        AnimationSequence& padding_y_by(float value, TransitionSpec transition = {});
        AnimationSequence& margin_x_by(float value, TransitionSpec transition = {});
        AnimationSequence& margin_y_by(float value, TransitionSpec transition = {});
        AnimationSequence& rotation_by(float radians, TransitionSpec transition = {});
        AnimationSequence& scale_by(ImVec2 factor, TransitionSpec transition = {});
        AnimationSequence& scale_by(float factor, TransitionSpec transition = {});
        AnimationSequence& color_by(ImVec4 value, TransitionSpec transition = {});
        AnimationSequence& border_color_by(ImVec4 value, TransitionSpec transition = {});
        AnimationSequence& background_color_by(ImVec4 value, TransitionSpec transition = {});

        AnimationSequence& release_padding_x(TransitionSpec transition = {});
        AnimationSequence& release_padding_y(TransitionSpec transition = {});
        AnimationSequence& release_margin_x(TransitionSpec transition = {});
        AnimationSequence& release_margin_y(TransitionSpec transition = {});
        AnimationSequence& release_rotation(TransitionSpec transition = {});
        AnimationSequence& release_scale(TransitionSpec transition = {});
        AnimationSequence& release_color(TransitionSpec transition = {});
        AnimationSequence& release_border_color(TransitionSpec transition = {});
        AnimationSequence& release_background_color(TransitionSpec transition = {});
        AnimationSequence& release_all(TransitionSpec transition = {});

        AnimationSequence& then(float delay = 0.0F);
        AnimationSequence& delay(float duration);
        AnimationSequence& end(std::function<void()> callback);

    private:
        friend class VisualState;

        explicit AnimationSequence(VisualState& state) : m_state(state) {}

        AnimationSequence& add(AnimationProperty property, std::optional<AnimationValue> value, TransitionSpec transition);
        AnimationSequence& add_color(AnimationProperty property, ImVec4 value, TransitionSpec transition);
        float current_float(AnimationProperty property) const;
        ImVec2 current_vec2(AnimationProperty property) const;

        VisualState& m_state;
        float m_cursor = 0.0F;
        float m_end = 0.0F;
    };
} // namespace ui
