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

AnimationSequence& AnimationSequence::rotation(float value, TransitionSpec transition) {
    return add(AnimationProperty::Rotation, value, transition);
}

AnimationSequence& AnimationSequence::scale(ImVec2 value, TransitionSpec transition) {
    return add(AnimationProperty::Scale, value, transition);
}

AnimationSequence& AnimationSequence::scale(float value, TransitionSpec transition) {
    return scale({value, value}, transition);
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

AnimationSequence& AnimationSequence::padding_x_by(float value, TransitionSpec transition) {
    return padding_x(std::max(0.0F, current_float(AnimationProperty::PaddingX) + value), transition);
}

AnimationSequence& AnimationSequence::padding_y_by(float value, TransitionSpec transition) {
    return padding_y(std::max(0.0F, current_float(AnimationProperty::PaddingY) + value), transition);
}

AnimationSequence& AnimationSequence::margin_x_by(float value, TransitionSpec transition) {
    return margin_x(std::max(0.0F, current_float(AnimationProperty::MarginX) + value), transition);
}

AnimationSequence& AnimationSequence::margin_y_by(float value, TransitionSpec transition) {
    return margin_y(std::max(0.0F, current_float(AnimationProperty::MarginY) + value), transition);
}

AnimationSequence& AnimationSequence::rotation_by(float radians, TransitionSpec transition) {
    return rotation(current_float(AnimationProperty::Rotation) + radians, transition);
}

AnimationSequence& AnimationSequence::scale_by(ImVec2 factor, TransitionSpec transition) {
    const ImVec2 current = current_vec2(AnimationProperty::Scale);
    return scale({current.x * factor.x, current.y * factor.y}, transition);
}

AnimationSequence& AnimationSequence::scale_by(float factor, TransitionSpec transition) {
    return scale_by({factor, factor}, transition);
}

AnimationSequence& AnimationSequence::color_by(ImVec4 value, TransitionSpec transition) {
    return add_color(AnimationProperty::Color, value, transition);
}

AnimationSequence& AnimationSequence::border_color_by(ImVec4 value, TransitionSpec transition) {
    return add_color(AnimationProperty::BorderColor, value, transition);
}

AnimationSequence& AnimationSequence::background_color_by(ImVec4 value, TransitionSpec transition) {
    return add_color(AnimationProperty::BackgroundColor, value, transition);
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

AnimationSequence& AnimationSequence::release_rotation(TransitionSpec transition) {
    return add(AnimationProperty::Rotation, std::nullopt, transition);
}

AnimationSequence& AnimationSequence::release_scale(TransitionSpec transition) {
    return add(AnimationProperty::Scale, std::nullopt, transition);
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

AnimationSequence& AnimationSequence::release_all(TransitionSpec transition) {
    for (uint8_t property = 0; property < static_cast<uint8_t>(AnimationProperty::COUNT); ++property) {
        add(static_cast<AnimationProperty>(property), std::nullopt, transition);
    }

    return *this;
}

AnimationSequence& AnimationSequence::then(float delay) {
    m_cursor = std::max(m_cursor, m_end) + std::max(0.0F, delay);
    return *this;
}

AnimationSequence& AnimationSequence::delay(float duration) {
    m_cursor += std::max(0.0F, duration);
    return *this;
}

AnimationSequence& AnimationSequence::end(std::function<void()> callback) {
    m_state.schedule_animation_callback(std::max(m_cursor, m_end), std::move(callback));
    return *this;
}

AnimationSequence&
AnimationSequence::add(AnimationProperty property, std::optional<AnimationValue> value, TransitionSpec transition) {
    transition.duration = std::max(0.0F, transition.duration);
    m_state.schedule_animation(property, value, m_cursor, transition);
    m_end = std::max(m_end, m_cursor + transition.duration);
    return *this;
}

AnimationSequence& AnimationSequence::add_color(AnimationProperty property, ImVec4 value, TransitionSpec transition) {
    const ImVec4 current = std::get<ImColor>(m_state.animation_value(m_state.computed_style(), property)).Value;
    return add(
        property,
        ImColor{
            std::clamp(current.x + value.x, 0.0F, 1.0F),
            std::clamp(current.y + value.y, 0.0F, 1.0F),
            std::clamp(current.z + value.z, 0.0F, 1.0F),
            std::clamp(current.w + value.w, 0.0F, 1.0F),
        },
        transition
    );
}

float AnimationSequence::current_float(AnimationProperty property) const {
    return std::get<float>(m_state.animation_value(m_state.computed_style(), property));
}

ImVec2 AnimationSequence::current_vec2(AnimationProperty property) const {
    return std::get<ImVec2>(m_state.animation_value(m_state.computed_style(), property));
}
