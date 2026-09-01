#pragma once

#include "../../../layout/geometry.hpp"

#include <imgui.h>

namespace ui {
    void begin_blur_frame();
    void draw_blur(ImDrawList& draw_list, Rect rect, int strength, float rounding, float opacity = 1.0F);
} // namespace ui
