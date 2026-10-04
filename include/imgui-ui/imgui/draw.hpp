#pragma once

#include "../layout/geometry.hpp"
#include "../style/computed-style.hpp"

#include <array>
#include <cstdint>
#include <imgui.h>
#include <optional>
#include <string_view>

namespace ui {
    class GenericValue;
    class EffectRegistry;

    enum class DrawListTarget : uint8_t {
        Window,
        Background,
        Foreground,
    };

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
        ImVec2 start;
        ImVec2 end;
        ImVec2 center;
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
    Rect viewport_work_area();

    /// returns the ancestor clip for effects, or fallback when no container scope is active.
    ImVec4 current_effect_clip(ImVec4 fallback);
    /// sets the clip used by descendant effects until pop_effect_clip().
    void push_effect_clip(ImVec4 clip);
    /// restores the previous descendant effect clip.
    void pop_effect_clip();

    void draw_line(ImDrawList& draw_list, ImVec2 start, ImVec2 end, const Color& color, float thickness);
    void draw_circle(ImDrawList& draw_list, ImVec2 center, float radius, const Color& color);
    void draw_circle_outline(ImDrawList& draw_list, ImVec2 center, float radius, const Color& color, float thickness);
    void draw_rect_filled(
        ImDrawList& draw_list, Rect rect, const Color& color, float rounding = 0.0F,
        ImDrawFlags flags = ImDrawFlags_RoundCornersAll
    );
    void draw_rect_outline(ImDrawList& draw_list, Rect rect, const Color& color, float thickness = 1.0F, float rounding = 0.0F);
    void draw_text(ImDrawList& draw_list, ImVec2 position, const Color& color, std::string_view text);
    void draw_text(
        ImDrawList& draw_list, ImVec2 position, const Color& source, const GenericValue& text, const ImVec4* clip_rect = nullptr
    );
    void
    draw_text_ellipsis(ImDrawList& draw_list, ImVec2 position, const Color& source, const GenericValue& text, ImVec4 clip_rect);
    void draw_triangle(
        ImDrawList& draw_list, ImVec2 center, ImVec2 size, const Color& color,
        TriangleDirection direction = TriangleDirection::Down
    );
    void draw_frame(
        ImDrawList& draw_list, Rect rect, const ComputedStyle& style, EffectRegistry* effects = nullptr, float opacity = 1.0F,
        const std::optional<Color>& background = {}
    );
    /// paints a frame without replaying shadow/blur already drawn by a container.
    void draw_frame_surface(ImDrawList& draw_list, Rect rect, const ComputedStyle& style, EffectRegistry* effects = nullptr);
    BorderPath rounded_rect_border_path(Rect rect, float rounding);
    void draw_border_path(
        ImDrawList& draw_list, const BorderPath& path, uint8_t border, const Color& color, float thickness, BorderStyle style
    );
    void draw_border(ImDrawList& draw_list, Rect rect, const ComputedStyle& style, const Color& color);
} // namespace ui
