#pragma once

#include "../layout/geometry.hpp"
#include "../style/style.hpp"

#include <array>
#include <cstdint>
#include <imgui.h>
#include <string_view>

namespace ui {
    class GenericValue;

    enum class DrawListTarget : uint8_t {
        /// uses the current imgui window draw list.
        Window,
        /// uses imgui's background draw list.
        Background,
        /// uses imgui's foreground draw list.
        Foreground,
    };

    /// points a triangle primitive toward one axis.
    enum class TriangleDirection : uint8_t {
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
        float length = 0.0F; // arc length. dash and dot placement uses this parameterization.
        uint8_t sides = BORDER_NONE;
    };

    struct BorderPath {
        // each corner is split between its adjacent sides so partial borders stop at the corner midpoint.
        std::array<BorderPathSegment, 12> segments;
    };

    ImDrawList& draw_list(DrawListTarget target = DrawListTarget::Window);

    void draw_line(ImDrawList& draw_list, ImVec2 start, ImVec2 end, ImColor color, float thickness);
    void draw_circle(ImDrawList& draw_list, ImVec2 center, float radius, ImColor color);
    void draw_circle_outline(ImDrawList& draw_list, ImVec2 center, float radius, ImColor color, float thickness);
    void draw_rect_filled(
        ImDrawList& draw_list, Rect rect, ImColor color, float rounding = 0.0F, ImDrawFlags flags = ImDrawFlags_RoundCornersAll
    );
    void draw_rect_filled_gradient(
        ImDrawList& draw_list, Rect rect, ImColor top_left, ImColor top_right, ImColor bottom_right, ImColor bottom_left
    );
    void draw_rect_outline(ImDrawList& draw_list, Rect rect, ImColor color, float thickness = 1.0F, float rounding = 0.0F);
    void draw_text(ImDrawList& draw_list, ImVec2 position, ImColor color, std::string_view text);
    void
    draw_text(ImDrawList& draw_list, ImVec2 position, ImColor color, const GenericValue& text, const ImVec4* clip_rect = nullptr);
    void draw_text_ellipsis(ImDrawList& draw_list, ImVec2 position, ImColor color, const GenericValue& text, ImVec4 clip_rect);
    void draw_triangle(
        ImDrawList& draw_list, ImVec2 center, ImVec2 size, ImColor color, TriangleDirection direction = TriangleDirection::Down
    );
    void draw_frame(ImDrawList& draw_list, Rect rect, const ComputedStyle& style);
    void draw_frame(ImDrawList& draw_list, Rect rect, const ComputedStyle& style, ImColor background);
    void draw_frame(ImDrawList& draw_list, Rect rect, const ComputedStyle& style, float opacity);
    void draw_frame_surface(ImDrawList& draw_list, Rect rect, const ComputedStyle& style, float opacity = 1.0F);
    BorderPath rounded_rect_border_path(Rect rect, float rounding);
    void draw_border_path(
        ImDrawList& draw_list, const BorderPath& path, uint8_t border, ImColor color, float thickness, BorderStyle style
    );
    void draw_border(ImDrawList& draw_list, Rect rect, const ComputedStyle& style, ImColor color);
} // namespace ui
