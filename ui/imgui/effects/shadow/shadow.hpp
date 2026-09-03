#pragma once

#include "../../../layout/geometry.hpp"
#include "../../../style/values.hpp"

#include <imgui.h>

namespace ui {
    struct BoxShadowRegion {
        Rect shape;
        Rect bounds;
        float rounding = 0.0F;
        float blur = 0.0F;
        ImVec4 color{};
    };

    void begin_box_shadow_frame();
    void draw_box_shadow(ImDrawList& draw_list, Rect rect, const BoxShadow& shadow, float rounding, float opacity = 1.0F);
    void set_box_shadow_callback(ImDrawCallback callback);
    void shutdown_box_shadow();
} // namespace ui
