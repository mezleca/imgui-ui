#pragma once

#include "blur.hpp"

namespace ui {
    struct BlurRegion {
        Rect rect;
        int strength = 0;
        float rounding = 0.0F;
        float opacity = 1.0F;
    };

    void set_blur_callback(ImDrawCallback callback);
    void shutdown_blur();
} // namespace ui
