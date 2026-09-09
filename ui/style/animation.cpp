#include "animation.hpp"

#include "state.hpp"

#include <algorithm>

using namespace ui;

StyleAnimationSequence& StyleAnimationSequence::padding_x(float value, TransitionSpec transition) {
    return add(m_state.slot(StyleAnimationProperty::PaddingX), value, transition);
}

StyleAnimationSequence& StyleAnimationSequence::padding_y(float value, TransitionSpec transition) {
    return add(m_state.slot(StyleAnimationProperty::PaddingY), value, transition);
}

StyleAnimationSequence& StyleAnimationSequence::margin_x(float value, TransitionSpec transition) {
    return add(m_state.slot(StyleAnimationProperty::MarginX), value, transition);
}

StyleAnimationSequence& StyleAnimationSequence::margin_y(float value, TransitionSpec transition) {
    return add(m_state.slot(StyleAnimationProperty::MarginY), value, transition);
}

StyleAnimationSequence& StyleAnimationSequence::rotation(float value, TransitionSpec transition) {
    return add(m_state.slot(StyleAnimationProperty::Rotation), value, transition);
}

StyleAnimationSequence& StyleAnimationSequence::scale(ImVec2 value, TransitionSpec transition) {
    return add(m_state.slot(StyleAnimationProperty::Scale), value, transition);
}

StyleAnimationSequence& StyleAnimationSequence::scale(float value, TransitionSpec transition) {
    return scale({value, value}, transition);
}

StyleAnimationSequence& StyleAnimationSequence::color(ImColor value, TransitionSpec transition) {
    return add(m_state.slot(StyleAnimationProperty::Color), value, transition);
}

StyleAnimationSequence& StyleAnimationSequence::border_color(ImColor value, TransitionSpec transition) {
    return add(m_state.slot(StyleAnimationProperty::BorderColor), value, transition);
}

StyleAnimationSequence& StyleAnimationSequence::background_color(ImColor value, TransitionSpec transition) {
    return add(m_state.slot(StyleAnimationProperty::BackgroundColor), value, transition);
}

StyleAnimationSequence& StyleAnimationSequence::padding_x_by(float value, TransitionSpec transition) {
    return padding_x(std::max(0.0F, current_float(m_state.slot(StyleAnimationProperty::PaddingX)) + value), transition);
}

StyleAnimationSequence& StyleAnimationSequence::padding_y_by(float value, TransitionSpec transition) {
    return padding_y(std::max(0.0F, current_float(m_state.slot(StyleAnimationProperty::PaddingY)) + value), transition);
}

StyleAnimationSequence& StyleAnimationSequence::margin_x_by(float value, TransitionSpec transition) {
    return margin_x(std::max(0.0F, current_float(m_state.slot(StyleAnimationProperty::MarginX)) + value), transition);
}

StyleAnimationSequence& StyleAnimationSequence::margin_y_by(float value, TransitionSpec transition) {
    return margin_y(std::max(0.0F, current_float(m_state.slot(StyleAnimationProperty::MarginY)) + value), transition);
}

StyleAnimationSequence& StyleAnimationSequence::rotation_by(float degrees, TransitionSpec transition) {
    return add_by(m_state.slot(StyleAnimationProperty::Rotation), degrees, transition);
}

StyleAnimationSequence& StyleAnimationSequence::scale_by(ImVec2 factor, TransitionSpec transition) {
    const ImVec2 current = current_vec2(m_state.slot(StyleAnimationProperty::Scale));
    return scale({current.x * factor.x, current.y * factor.y}, transition);
}

StyleAnimationSequence& StyleAnimationSequence::scale_by(float factor, TransitionSpec transition) {
    return scale_by({factor, factor}, transition);
}

StyleAnimationSequence& StyleAnimationSequence::color_by(ImVec4 value, TransitionSpec transition) {
    return add_by(m_state.slot(StyleAnimationProperty::Color), ImColor{value}, transition);
}

StyleAnimationSequence& StyleAnimationSequence::border_color_by(ImVec4 value, TransitionSpec transition) {
    return add_by(m_state.slot(StyleAnimationProperty::BorderColor), ImColor{value}, transition);
}

StyleAnimationSequence& StyleAnimationSequence::background_color_by(ImVec4 value, TransitionSpec transition) {
    return add_by(m_state.slot(StyleAnimationProperty::BackgroundColor), ImColor{value}, transition);
}

StyleAnimationSequence& StyleAnimationSequence::release_padding_x(TransitionSpec transition) {
    return release(m_state.slot(StyleAnimationProperty::PaddingX), transition);
}

StyleAnimationSequence& StyleAnimationSequence::release_padding_y(TransitionSpec transition) {
    return release(m_state.slot(StyleAnimationProperty::PaddingY), transition);
}

StyleAnimationSequence& StyleAnimationSequence::release_margin_x(TransitionSpec transition) {
    return release(m_state.slot(StyleAnimationProperty::MarginX), transition);
}

StyleAnimationSequence& StyleAnimationSequence::release_margin_y(TransitionSpec transition) {
    return release(m_state.slot(StyleAnimationProperty::MarginY), transition);
}

StyleAnimationSequence& StyleAnimationSequence::release_rotation(TransitionSpec transition) {
    return release(m_state.slot(StyleAnimationProperty::Rotation), transition);
}

StyleAnimationSequence& StyleAnimationSequence::release_scale(TransitionSpec transition) {
    return release(m_state.slot(StyleAnimationProperty::Scale), transition);
}

StyleAnimationSequence& StyleAnimationSequence::release_color(TransitionSpec transition) {
    return release(m_state.slot(StyleAnimationProperty::Color), transition);
}

StyleAnimationSequence& StyleAnimationSequence::release_border_color(TransitionSpec transition) {
    return release(m_state.slot(StyleAnimationProperty::BorderColor), transition);
}

StyleAnimationSequence& StyleAnimationSequence::release_background_color(TransitionSpec transition) {
    return release(m_state.slot(StyleAnimationProperty::BackgroundColor), transition);
}

StyleAnimationSequence& StyleAnimationSequence::release_all(TransitionSpec transition) {
    for (StyleAnimationSlot& slot : m_state.animation_slots()) {
        release(slot, transition);
    }
    return *this;
}

StyleAnimationSequence& StyleAnimationSequence::then(float delay) {
    m_sequence.then(delay);
    return *this;
}

StyleAnimationSequence& StyleAnimationSequence::delay(float duration) {
    m_sequence.delay(duration);
    return *this;
}

StyleAnimationSequence& StyleAnimationSequence::end(std::function<void()> callback) {
    m_sequence.end(std::move(callback));
    return *this;
}

StyleAnimationSequence& StyleAnimationSequence::add(StyleAnimationSlot& slot, AnimationValue value, TransitionSpec transition) {
    m_sequence.to(m_state.target(slot), std::move(value), transition);
    return *this;
}

StyleAnimationSequence&
StyleAnimationSequence::add_by(StyleAnimationSlot& slot, AnimationValue value, TransitionSpec transition) {
    m_sequence.by(m_state.target(slot), std::move(value), transition);
    return *this;
}

StyleAnimationSequence& StyleAnimationSequence::release(StyleAnimationSlot& slot, TransitionSpec transition) {
    m_sequence.release(m_state.target(slot), transition);
    return *this;
}

float StyleAnimationSequence::current_float(StyleAnimationSlot& slot) const {
    return std::get<float>(slot.current);
}

ImVec2 StyleAnimationSequence::current_vec2(StyleAnimationSlot& slot) const {
    return std::get<ImVec2>(slot.current);
}
