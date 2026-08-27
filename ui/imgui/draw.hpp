#pragma once

#include "../layout/geometry.hpp"

#include <imgui.h>
#include <string_view>

namespace ui {
    class Style;

    enum class TriangleDirection {
        Up,
        Down,
        Left,
        Right,
    };

    void draw_line(ImVec2 start, ImVec2 end, ImColor color, float thickness);
    void draw_circle(ImVec2 center, float radius, ImColor color);
    void draw_circle_outline(ImVec2 center, float radius, ImColor color, float thickness);
    void draw_text(ImVec2 position, ImColor color, std::string_view text);
    void draw_triangle(ImVec2 center, ImVec2 size, ImColor color, TriangleDirection direction = TriangleDirection::Down);

    void draw_frame(Rect rect, const Style& style);
    void draw_frame(Rect rect, const Style& style, ImColor background);
    void draw_border(Rect rect, const Style& style);
} // namespace ui
