#pragma once

#include <cmath>

namespace ui {
    using EasingFunction = float (*)(float);

    namespace easing {
        inline float linear(float progress) {
            return progress;
        }

        inline float in_quad(float progress) {
            return progress * progress;
        }

        inline float out_quad(float progress) {
            return progress * (2.0F - progress);
        }

        inline float in_out_quad(float progress) {
            return progress < 0.5F ? 2.0F * progress * progress : 1.0F - std::pow(-2.0F * progress + 2.0F, 2.0F) / 2.0F;
        }

        inline float in_cubic(float progress) {
            return progress * progress * progress;
        }

        inline float out_cubic(float progress) {
            return 1.0F - std::pow(1.0F - progress, 3.0F);
        }

        inline float in_out_cubic(float progress) {
            return progress < 0.5F ? 4.0F * progress * progress * progress
                                   : 1.0F - std::pow(-2.0F * progress + 2.0F, 3.0F) / 2.0F;
        }

        inline float in_sine(float progress) {
            return 1.0F - std::cos(progress * 1.57079632679F);
        }

        inline float out_sine(float progress) {
            return std::sin(progress * 1.57079632679F);
        }

        inline float in_out_sine(float progress) {
            return -(std::cos(3.14159265359F * progress) - 1.0F) / 2.0F;
        }

        inline float in_back(float progress) {
            constexpr float overshoot = 1.70158F;
            return (overshoot + 1.0F) * progress * progress * progress - overshoot * progress * progress;
        }

        inline float out_back(float progress) {
            constexpr float overshoot = 1.70158F;
            const float shifted = progress - 1.0F;
            return 1.0F + (overshoot + 1.0F) * shifted * shifted * shifted + overshoot * shifted * shifted;
        }

        inline float in_out_back(float progress) {
            constexpr float overshoot = 1.70158F * 1.525F;
            const float scaled = progress * 2.0F;
            if (scaled < 1.0F) {
                return scaled * scaled * ((overshoot + 1.0F) * scaled - overshoot) / 2.0F;
            }

            const float shifted = scaled - 2.0F;
            return (shifted * shifted * ((overshoot + 1.0F) * shifted + overshoot) + 2.0F) / 2.0F;
        }
    } // namespace easing

    struct TransitionSpec {
        float duration = 0.0F;
        EasingFunction easing = easing::linear;
    };
} // namespace ui
