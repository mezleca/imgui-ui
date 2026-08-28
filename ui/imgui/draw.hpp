#pragma once

#include "../layout/geometry.hpp"
#include "../style/style.hpp"

#include <array>
#include <imgui.h>
#include <string_view>

namespace ui {
    enum class TriangleDirection {
        Up,
        Down,
        Left,
        Right,
    };

    enum class BorderPathSegmentType : uint8_t {
        Line,
        Arc,
    };

    struct BorderPathSegment {
        BorderPathSegmentType type = BorderPathSegmentType::Line;
        ImVec2 start{};
        ImVec2 end{};
        ImVec2 center{};
        float start_angle = 0.0F;
        float end_angle = 0.0F;
        float length = 0.0F; // arc length; dash and dot placement uses this parameterization.
        uint8_t sides = BORDER_NONE;
    };

    struct BorderPath {
        // each corner is split between its adjacent sides so partial borders stop at the corner midpoint.
        std::array<BorderPathSegment, 12> segments;
    };

    void draw_line(ImVec2 start, ImVec2 end, ImColor color, float thickness);
    void draw_circle(ImVec2 center, float radius, ImColor color);
    void draw_circle_outline(ImVec2 center, float radius, ImColor color, float thickness);
    void draw_text(ImVec2 position, ImColor color, std::string_view text);
    void draw_triangle(ImVec2 center, ImVec2 size, ImColor color, TriangleDirection direction = TriangleDirection::Down);
    void draw_frame(Rect rect, const Style& style);
    void draw_frame(Rect rect, const Style& style, ImColor background);
    void draw_frame(Rect rect, const Style& style, float opacity);
    // builds a clockwise path parameterized by arc length.
    BorderPath rounded_rect_border_path(Rect rect, float rounding);
    void draw_border_path(const BorderPath& path, uint8_t border, ImColor color, float thickness, BorderStyle style);
    void draw_border(Rect rect, const Style& style);
    void draw_border(Rect rect, const Style& style, ImColor color);
} // namespace ui
