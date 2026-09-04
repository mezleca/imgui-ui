#pragma once

#include "../layout/geometry.hpp"
#include "../style/style.hpp"

#include <array>
#include <imgui.h>
#include <string_view>

namespace ui {
    class GenericValue;

    enum class DrawListTarget {
        Window,
        Background,
        Foreground,
    };

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
        float length = 0.0F; // arc length. dash and dot placement uses this parameterization.
        uint8_t sides = BORDER_NONE;
    };

    struct BorderPath {
        // each corner is split between its adjacent sides so partial borders stop at the corner midpoint.
        std::array<BorderPathSegment, 12> segments;
    };

    ImDrawList& draw_list(DrawListTarget target = DrawListTarget::Window);

    void draw_line(ImDrawList& draw_list, ImVec2 start, ImVec2 end, ImColor color, float thickness);
    void draw_line(ImVec2 start, ImVec2 end, ImColor color, float thickness, DrawListTarget target = DrawListTarget::Window);
    void draw_circle(ImDrawList& draw_list, ImVec2 center, float radius, ImColor color);
    void draw_circle(ImVec2 center, float radius, ImColor color, DrawListTarget target = DrawListTarget::Window);
    void draw_circle_outline(ImDrawList& draw_list, ImVec2 center, float radius, ImColor color, float thickness);
    void draw_circle_outline(
        ImVec2 center, float radius, ImColor color, float thickness, DrawListTarget target = DrawListTarget::Window
    );
    void draw_rect_filled(
        ImDrawList& draw_list, Rect rect, ImColor color, float rounding = 0.0F, ImDrawFlags flags = ImDrawFlags_RoundCornersAll
    );
    void draw_text(ImDrawList& draw_list, ImVec2 position, ImColor color, std::string_view text);
    void draw_text(ImVec2 position, ImColor color, std::string_view text, DrawListTarget target = DrawListTarget::Window);
    void
    draw_text(ImDrawList& draw_list, ImVec2 position, ImColor color, const GenericValue& text, const ImVec4* clip_rect = nullptr);
    void draw_text(
        ImVec2 position, ImColor color, const GenericValue& text, const ImVec4* clip_rect = nullptr,
        DrawListTarget target = DrawListTarget::Window
    );
    void draw_text_ellipsis(ImDrawList& draw_list, ImVec2 position, ImColor color, const GenericValue& text, ImVec4 clip_rect);
    void draw_text_ellipsis(
        ImVec2 position, ImColor color, const GenericValue& text, ImVec4 clip_rect, DrawListTarget target = DrawListTarget::Window
    );
    void draw_triangle(
        ImDrawList& draw_list, ImVec2 center, ImVec2 size, ImColor color, TriangleDirection direction = TriangleDirection::Down
    );
    void draw_triangle(
        ImVec2 center, ImVec2 size, ImColor color, TriangleDirection direction = TriangleDirection::Down,
        DrawListTarget target = DrawListTarget::Window
    );
    void draw_frame(ImDrawList& draw_list, Rect rect, const Style& style);
    void draw_frame(Rect rect, const Style& style, DrawListTarget target = DrawListTarget::Window);
    void draw_frame(ImDrawList& draw_list, Rect rect, const Style& style, ImColor background);
    void draw_frame(Rect rect, const Style& style, ImColor background, DrawListTarget target = DrawListTarget::Window);
    void draw_frame(ImDrawList& draw_list, Rect rect, const Style& style, float opacity);
    void draw_frame(Rect rect, const Style& style, float opacity, DrawListTarget target = DrawListTarget::Window);
    void draw_frame_surface(ImDrawList& draw_list, Rect rect, const Style& style, float opacity = 1.0F);
    BorderPath rounded_rect_border_path(Rect rect, float rounding);
    void draw_border_path(
        ImDrawList& draw_list, const BorderPath& path, uint8_t border, ImColor color, float thickness, BorderStyle style
    );
    void draw_border_path(const BorderPath& path, uint8_t border, ImColor color, float thickness, BorderStyle style);
    void draw_border(ImDrawList& draw_list, Rect rect, const Style& style, ImColor color);
    void draw_border(Rect rect, const Style& style, DrawListTarget target = DrawListTarget::Window);
    void draw_border(Rect rect, const Style& style, ImColor color, DrawListTarget target = DrawListTarget::Window);
} // namespace ui
