#include "animation.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace ui;

struct AnimationStep {
    AnimationTarget target;
    AnimationValue value;
    TransitionSpec transition;
    float start = 0.0F;
    bool relative = false;
    bool release = false;
};

struct AnimationTrack {
    AnimationTarget target;
    AnimationValue start;
    AnimationValue target_value;
    TransitionSpec transition;
    float started_at = 0.0F;
    bool release = false;
};

struct AnimationCallback {
    float at = 0.0F;
    std::function<void()> callback;
};

struct ui::AnimatorState {
    std::vector<AnimationStep> steps;
    std::vector<AnimationTrack> tracks;
    std::vector<AnimationCallback> callbacks;
    float time = 0.0F;
};

static AnimationValue interpolate_animation_value(const AnimationValue& start, const AnimationValue& target, float progress) {
    if (const auto* start_float = std::get_if<float>(&start); start_float != nullptr) {
        return std::lerp(*start_float, std::get<float>(target), progress);
    }

    if (const auto* start_vec2 = std::get_if<ImVec2>(&start); start_vec2 != nullptr) {
        const ImVec2& target_vec2 = std::get<ImVec2>(target);
        return ImVec2{
            std::lerp(start_vec2->x, target_vec2.x, progress),
            std::lerp(start_vec2->y, target_vec2.y, progress),
        };
    }

    const ImVec4& start_color = std::get<ImColor>(start).Value;
    const ImVec4& target_color = std::get<ImColor>(target).Value;
    return ImColor{
        std::lerp(start_color.x, target_color.x, progress),
        std::lerp(start_color.y, target_color.y, progress),
        std::lerp(start_color.z, target_color.z, progress),
        std::lerp(start_color.w, target_color.w, progress),
    };
}

static AnimationValue add_animation_value(const AnimationValue& value, const AnimationValue& amount) {
    if (const auto* float_value = std::get_if<float>(&value); float_value != nullptr) {
        return *float_value + std::get<float>(amount);
    }

    if (const auto* vec2_value = std::get_if<ImVec2>(&value); vec2_value != nullptr) {
        const ImVec2& amount_vec2 = std::get<ImVec2>(amount);
        return ImVec2{vec2_value->x + amount_vec2.x, vec2_value->y + amount_vec2.y};
    }

    const ImVec4& color = std::get<ImColor>(value).Value;
    const ImVec4& color_amount = std::get<ImColor>(amount).Value;
    return ImColor{
        std::clamp(color.x + color_amount.x, 0.0F, 1.0F),
        std::clamp(color.y + color_amount.y, 0.0F, 1.0F),
        std::clamp(color.z + color_amount.z, 0.0F, 1.0F),
        std::clamp(color.w + color_amount.w, 0.0F, 1.0F),
    };
}

static AnimationValue animation_track_value(const AnimationTrack& track, float time) {
    const float duration = std::max(0.0F, track.transition.duration);
    const float elapsed = std::clamp(time - track.started_at, 0.0F, duration);
    const float progress =
        duration == 0.0F ? 1.0F
                         : (track.transition.easing != nullptr ? track.transition.easing : easing::linear)(elapsed / duration);
    return interpolate_animation_value(track.start, track.target_value, progress);
}

static bool animation_track_complete(const AnimationTrack& track, float time) {
    return time + 0.00001F >= track.started_at + std::max(0.0F, track.transition.duration);
}

Animator::Animator() = default;
Animator::~Animator() = default;
Animator::Animator(Animator&&) noexcept = default;

Animator& Animator::operator=(Animator&&) noexcept = default;

AnimationSequence Animator::animate() {
    return AnimationSequence{*this, time()};
}

void Animator::update(float dt) {
    if (m_state == nullptr) return;

    AnimatorState& state = *m_state;
    state.time += std::max(0.0F, dt);

    // start every due step before writing tracks so a later step can use the current output of the track it replaces.
    for (auto step_it = state.steps.begin(); step_it != state.steps.end();) {
        if (step_it->start > state.time) {
            ++step_it;
            continue;
        }

        const auto track_it = std::find_if(state.tracks.begin(), state.tracks.end(), [step_it](const AnimationTrack& track) {
            return track.target.identity == step_it->target.identity;
        });
        const AnimationValue start = track_it == state.tracks.end() ? step_it->target.read(step_it->target.context)
                                                                    : animation_track_value(*track_it, state.time);
        const AnimationValue target = step_it->release
                                          ? step_it->target.base(step_it->target.context)
                                          : (step_it->relative ? add_animation_value(start, step_it->value) : step_it->value);

        // each target has one track, so replacing it starts from the exact value it would draw in this frame.
        if (track_it != state.tracks.end()) {
            state.tracks.erase(track_it);
        }

        state.tracks.push_back({step_it->target, start, target, step_it->transition, step_it->start, step_it->release});
        step_it = state.steps.erase(step_it);
    }

    for (auto track_it = state.tracks.begin(); track_it != state.tracks.end();) {
        const AnimationValue value = animation_track_value(*track_it, state.time);
        track_it->target.write(track_it->target.context, value);

        if (!animation_track_complete(*track_it, state.time)) {
            ++track_it;
            continue;
        }

        if (track_it->release) {
            track_it->target.release(track_it->target.context);
        }
        track_it = state.tracks.erase(track_it);
    }

    // remove due callbacks before invoking user code because a callback may schedule more work on this animator.
    std::vector<std::function<void()>> callbacks;
    for (auto callback_it = state.callbacks.begin(); callback_it != state.callbacks.end();) {
        if (callback_it->at > state.time) {
            ++callback_it;
            continue;
        }

        callbacks.push_back(std::move(callback_it->callback));
        callback_it = state.callbacks.erase(callback_it);
    }

    for (const auto& callback : callbacks) {
        callback();
    }
}

void Animator::cancel() {
    if (m_state == nullptr) return;

    AnimatorState& state = *m_state;
    state.steps.clear();
    state.tracks.clear();
    state.callbacks.clear();
}

bool Animator::transitioning() const {
    return m_state != nullptr && (!m_state->steps.empty() || !m_state->tracks.empty() || !m_state->callbacks.empty());
}

void Animator::schedule(AnimationTarget target, AnimationValue value, bool relative, float start, TransitionSpec transition) {
    if (target.read == nullptr || target.write == nullptr || target.identity == nullptr) {
        return;
    }

    const float scheduled_at = std::max(start, time());
    AnimatorState& animator_state = ensure_state();
    animator_state.steps.push_back({target, std::move(value), transition, scheduled_at, relative, false});
}

void Animator::schedule_release(AnimationTarget target, float start, TransitionSpec transition) {
    if (target.read == nullptr || target.base == nullptr || target.write == nullptr || target.release == nullptr ||
        target.identity == nullptr) {
        return;
    }

    const float scheduled_at = std::max(start, time());
    AnimatorState& animator_state = ensure_state();
    animator_state.steps.push_back({target, 0.0F, transition, scheduled_at, false, true});
}

void Animator::schedule_callback(float at, std::function<void()> callback) {
    if (callback) {
        const float scheduled_at = std::max(at, time());
        AnimatorState& animator_state = ensure_state();
        animator_state.callbacks.push_back({scheduled_at, std::move(callback)});
    }
}

float Animator::time() const {
    return m_state != nullptr ? m_state->time : 0.0F;
}

AnimatorState& Animator::ensure_state() {
    if (m_state == nullptr) m_state = std::make_unique<AnimatorState>();
    return *m_state;
}

AnimationSequence& AnimationSequence::to(AnimationTarget target, AnimationValue value, TransitionSpec transition) {
    transition.duration = std::max(0.0F, transition.duration);
    m_animator.schedule(target, std::move(value), false, m_cursor, transition);
    m_end = std::max(m_end, m_cursor + transition.duration);
    return *this;
}

AnimationSequence& AnimationSequence::by(AnimationTarget target, AnimationValue value, TransitionSpec transition) {
    transition.duration = std::max(0.0F, transition.duration);
    m_animator.schedule(target, std::move(value), true, m_cursor, transition);
    m_end = std::max(m_end, m_cursor + transition.duration);
    return *this;
}

AnimationSequence& AnimationSequence::release(AnimationTarget target, TransitionSpec transition) {
    transition.duration = std::max(0.0F, transition.duration);
    m_animator.schedule_release(target, m_cursor, transition);
    m_end = std::max(m_end, m_cursor + transition.duration);
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
    m_animator.schedule_callback(std::max(m_cursor, m_end), std::move(callback));
    return *this;
}
