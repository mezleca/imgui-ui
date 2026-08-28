#pragma once

#include "../layout/geometry.hpp"

#include <imgui.h>

namespace ui {
    struct BlurRegion {
        Rect rect;
        int strength = 0;
        float rounding = 0.0F;
        float opacity = 1.0F;
    };

    void begin_blur_frame();
    void set_blur_callback(ImDrawCallback callback);
    void draw_blur(Rect rect, int strength, float rounding, float opacity = 1.0F);
} // namespace ui
