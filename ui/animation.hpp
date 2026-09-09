#pragma once

#include "transition.hpp"

#include <concepts>
#include <functional>
#include <memory>
#include <variant>

#include <imgui.h>

namespace ui {
    using AnimationValue = std::variant<float, ImVec2, ImColor>;

    /// connects one mutable value to the animator without copying it.
    struct AnimationTarget {
        // reads the value displayed when a track starts.
        AnimationValue (*read)(void*) = nullptr;
        // reads the value restored after a release track completes.
        AnimationValue (*base)(void*) = nullptr;
        // writes the value interpolated for the current frame.
        void (*write)(void*, const AnimationValue&) = nullptr;
        // restores the target after a release track completes.
        void (*release)(void*) = nullptr;
        void* context = nullptr;
        const void* identity = nullptr;
    };

    template <typename T>
    concept Animatable = std::same_as<T, float> || std::same_as<T, ImVec2> || std::same_as<T, ImColor>;

    template <Animatable T>
    AnimationValue read_animation_reference(void* context) {
        return *static_cast<T*>(context);
    }

    template <Animatable T>
    void write_animation_reference(void* context, const AnimationValue& value) {
        *static_cast<T*>(context) = std::get<T>(value);
    }

    template <Animatable T>
    AnimationTarget animation_target(T& value) {
        return {
            .read = &read_animation_reference<T>,
            .write = &write_animation_reference<T>,
            .context = &value,
            .identity = &value,
        };
    }

    class Animator;
    struct AnimatorState;

    /// schedules ordered tracks for one animator.
    class AnimationSequence final {
    public:
        template <Animatable T>
        AnimationSequence& to(T& value, T target, TransitionSpec transition = {}) {
            return to(animation_target(value), target, transition);
        }

        template <Animatable T>
        AnimationSequence& by(T& value, T amount, TransitionSpec transition = {}) {
            return by(animation_target(value), amount, transition);
        }

        AnimationSequence& to(AnimationTarget target, AnimationValue value, TransitionSpec transition = {});
        AnimationSequence& by(AnimationTarget target, AnimationValue value, TransitionSpec transition = {});
        AnimationSequence& release(AnimationTarget target, TransitionSpec transition = {});
        AnimationSequence& then(float delay = 0.0F);
        AnimationSequence& delay(float duration);
        AnimationSequence& end(std::function<void()> callback);

    private:
        friend class Animator;

        explicit AnimationSequence(Animator& animator, float start) : m_animator(animator), m_cursor(start), m_end(start) {}

        Animator& m_animator;
        float m_cursor = 0.0F;
        float m_end = 0.0F;
    };

    /// advances value tracks and completion callbacks using elapsed frame time.
    class Animator final {
    public:
        Animator();
        ~Animator();
        Animator(const Animator&) = delete;
        Animator& operator=(const Animator&) = delete;
        Animator(Animator&&) noexcept;
        Animator& operator=(Animator&&) noexcept;

        AnimationSequence animate();
        void update(float dt);
        void cancel();

        bool transitioning() const;

    private:
        friend class AnimationSequence;

        void schedule(AnimationTarget target, AnimationValue value, bool relative, float start, TransitionSpec transition);
        void schedule_release(AnimationTarget target, float start, TransitionSpec transition);
        void schedule_callback(float at, std::function<void()> callback);
        float time() const;
        AnimatorState& ensure_state();

        std::unique_ptr<AnimatorState> m_state;
    };
} // namespace ui
