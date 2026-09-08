#include "animation.hpp"

#include "state.hpp"

#include <algorithm>

using namespace ui;

AnimationSequence& AnimationSequence::padding_x(float value, TransitionSpec transition) {
    return add(AnimationProperty::PaddingX, value, transition);
}

AnimationSequence& AnimationSequence::padding_y(float value, TransitionSpec transition) {
    return add(AnimationProperty::PaddingY, value, transition);
}

AnimationSequence& AnimationSequence::margin_x(float value, TransitionSpec transition) {
    return add(AnimationProperty::MarginX, value, transition);
}

AnimationSequence& AnimationSequence::margin_y(float value, TransitionSpec transition) {
    return add(AnimationProperty::MarginY, value, transition);
}

AnimationSequence& AnimationSequence::color(ImColor value, TransitionSpec transition) {
    return add(AnimationProperty::Color, value, transition);
}

AnimationSequence& AnimationSequence::border_color(ImColor value, TransitionSpec transition) {
    return add(AnimationProperty::BorderColor, value, transition);
}

AnimationSequence& AnimationSequence::background_color(ImColor value, TransitionSpec transition) {
    return add(AnimationProperty::BackgroundColor, value, transition);
}

AnimationSequence& AnimationSequence::release_padding_x(TransitionSpec transition) {
    return add(AnimationProperty::PaddingX, std::nullopt, transition);
}

AnimationSequence& AnimationSequence::release_padding_y(TransitionSpec transition) {
    return add(AnimationProperty::PaddingY, std::nullopt, transition);
}

AnimationSequence& AnimationSequence::release_margin_x(TransitionSpec transition) {
    return add(AnimationProperty::MarginX, std::nullopt, transition);
}

AnimationSequence& AnimationSequence::release_margin_y(TransitionSpec transition) {
    return add(AnimationProperty::MarginY, std::nullopt, transition);
}

AnimationSequence& AnimationSequence::release_color(TransitionSpec transition) {
    return add(AnimationProperty::Color, std::nullopt, transition);
}

AnimationSequence& AnimationSequence::release_border_color(TransitionSpec transition) {
    return add(AnimationProperty::BorderColor, std::nullopt, transition);
}

AnimationSequence& AnimationSequence::release_background_color(TransitionSpec transition) {
    return add(AnimationProperty::BackgroundColor, std::nullopt, transition);
}

AnimationSequence& AnimationSequence::then(float delay) {
    m_cursor = std::max(m_cursor, m_end) + std::max(0.0F, delay);
    return *this;
}

AnimationSequence& AnimationSequence::delay(float duration) {
    m_cursor += std::max(0.0F, duration);
    return *this;
}

AnimationSequence&
AnimationSequence::add(AnimationProperty property, std::optional<AnimationValue> value, TransitionSpec transition) {
    transition.duration = std::max(0.0F, transition.duration);
    m_state.schedule_animation(property, std::move(value), m_cursor, transition);
    m_end = std::max(m_end, m_cursor + transition.duration);
    return *this;
}
