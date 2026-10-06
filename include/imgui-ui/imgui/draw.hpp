#pragma once

#include "border.hpp"
#include "painter.hpp"

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

    ImDrawList& draw_list(DrawListTarget target = DrawListTarget::Window);
    Rect viewport_work_area();

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
} // namespace ui
