#include "state.hpp"

#include <algorithm>
#include <cmath>

using namespace ui;

AnimationSequence VisualState::animate() {
    cancel_animations();
    return AnimationSequence{*this};
}

void VisualState::cancel_animations() {
    const bool layout_changed = std::any_of(
                                    m_animation_tracks.begin(), m_animation_tracks.end(),
                                    [](const AnimationTrack& track) { return VisualState::affects_layout(track.property); }
                                ) ||
                                has_layout_override();

    m_animation_steps.clear();
    m_animation_tracks.clear();
    m_animation_overrides.fill(std::nullopt);
    m_animation_time = 0.0F;
    m_has_presentation_style = false;

    if (layout_changed) {
        m_presentation_style.notify_change();
    }
}

void VisualState::schedule_animation(
    AnimationProperty property, std::optional<AnimationValue> value, float start, TransitionSpec transition
) {
    m_animation_steps.push_back({property, std::move(value), transition, std::max(0.0F, start)});
}

void VisualState::update_animations(float dt) {
    m_presentation_style = style();
    for (size_t index = 0; index < m_animation_overrides.size(); ++index) {
        if (m_animation_overrides[index].has_value()) {
            apply_animation_value(m_presentation_style, static_cast<AnimationProperty>(index), *m_animation_overrides[index]);
        }
    }

    if (m_animation_steps.empty() && m_animation_tracks.empty()) {
        m_has_presentation_style = has_animation_overrides();
        return;
    }

    m_animation_time += std::max(0.0F, dt);
    bool layout_changed = false;

    for (auto step_it = m_animation_steps.begin(); step_it != m_animation_steps.end();) {
        if (step_it->start > m_animation_time) {
            ++step_it;
            continue;
        }

        const AnimationProperty property = step_it->property;
        const AnimationValue start = animation_value(m_presentation_style, property);
        const AnimationValue target = step_it->value.has_value() ? *step_it->value : animation_value(style(), property);

        std::erase_if(m_animation_tracks, [property](const AnimationTrack& track) { return track.property == property; });
        m_animation_tracks.push_back({property, start, target, step_it->transition, step_it->start, !step_it->value.has_value()});
        layout_changed = layout_changed || affects_layout(property);
        step_it = m_animation_steps.erase(step_it);
    }

    for (auto track_it = m_animation_tracks.begin(); track_it != m_animation_tracks.end();) {
        AnimationTrack& track = *track_it;
        const bool complete = m_animation_time + 0.00001F >= track.started_at + track.transition.duration;
        const float elapsed =
            complete ? track.transition.duration : std::min(track.transition.duration, m_animation_time - track.started_at);
        const float progress = track.transition.duration == 0.0F
                                   ? 1.0F
                                   : (track.transition.easing != nullptr ? track.transition.easing
                                                                         : easing::linear)(elapsed / track.transition.duration);

        AnimationValue value;
        if (const auto* start = std::get_if<float>(&track.start); start != nullptr) {
            value = std::lerp(*start, std::get<float>(track.target), progress);
        } else {
            const ImVec4& start_value = std::get<ImColor>(track.start).Value;
            const ImVec4& target = std::get<ImColor>(track.target).Value;
            value = ImColor{
                std::lerp(start_value.x, target.x, progress),
                std::lerp(start_value.y, target.y, progress),
                std::lerp(start_value.z, target.z, progress),
                std::lerp(start_value.w, target.w, progress),
            };
        }

        apply_animation_value(m_presentation_style, track.property, value);
        layout_changed = layout_changed || affects_layout(track.property);

        if (!complete) {
            ++track_it;
            continue;
        }

        std::optional<AnimationValue>& override = m_animation_overrides[static_cast<size_t>(track.property)];
        if (track.release) {
            override.reset();
        } else {
            override = track.target;
        }
        track_it = m_animation_tracks.erase(track_it);
    }

    if (layout_changed) {
        m_presentation_style.notify_change();
    }

    m_has_presentation_style = has_animation_overrides() || !m_animation_tracks.empty();
}

void VisualState::apply_animation_value(Style& style, AnimationProperty property, const AnimationValue& value) const {
    switch (property) {
        case AnimationProperty::PaddingX:
            style.m_padding.value.x = std::max(0.0F, std::get<float>(value));
            return;
        case AnimationProperty::PaddingY:
            style.m_padding.value.y = std::max(0.0F, std::get<float>(value));
            return;
        case AnimationProperty::MarginX:
            style.m_margin.value.x = std::max(0.0F, std::get<float>(value));
            return;
        case AnimationProperty::MarginY:
            style.m_margin.value.y = std::max(0.0F, std::get<float>(value));
            return;
        case AnimationProperty::Color:
            style.m_color.value = std::get<ImColor>(value);
            return;
        case AnimationProperty::BorderColor:
            style.m_border_color.value = std::get<ImColor>(value);
            return;
        case AnimationProperty::BackgroundColor:
            style.m_background_color.value = std::get<ImColor>(value);
            return;
        case AnimationProperty::_COUNT:
            return;
    }
}

AnimationValue VisualState::animation_value(const ComputedStyle& style, AnimationProperty property) const {
    switch (property) {
        case AnimationProperty::PaddingX:
            return style.padding().x;
        case AnimationProperty::PaddingY:
            return style.padding().y;
        case AnimationProperty::MarginX:
            return style.margin().x;
        case AnimationProperty::MarginY:
            return style.margin().y;
        case AnimationProperty::Color:
            return style.color().value;
        case AnimationProperty::BorderColor:
            return style.border_color().value;
        case AnimationProperty::BackgroundColor:
            return style.background_color().value;
        case AnimationProperty::_COUNT:
            return 0.0F;
    }

    return 0.0F;
}

bool VisualState::has_animation_overrides() const {
    return std::any_of(m_animation_overrides.begin(), m_animation_overrides.end(), [](const auto& value) {
        return value.has_value();
    });
}

bool VisualState::has_layout_override() const {
    return m_animation_overrides[static_cast<size_t>(AnimationProperty::PaddingX)].has_value() ||
           m_animation_overrides[static_cast<size_t>(AnimationProperty::PaddingY)].has_value() ||
           m_animation_overrides[static_cast<size_t>(AnimationProperty::MarginX)].has_value() ||
           m_animation_overrides[static_cast<size_t>(AnimationProperty::MarginY)].has_value();
}

bool VisualState::affects_layout(AnimationProperty property) {
    return property == AnimationProperty::PaddingX || property == AnimationProperty::PaddingY ||
           property == AnimationProperty::MarginX || property == AnimationProperty::MarginY;
}
