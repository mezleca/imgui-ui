#include "draw.hpp"

#include "../style/style.hpp"

namespace ui {
    void draw_line(ImVec2 start, ImVec2 end, ImColor color, float thickness) {
        ImGui::GetWindowDrawList()->AddLine(start, end, color, thickness);
    }

    void draw_text(ImVec2 position, ImColor color, std::string_view text) {
        ImGui::GetWindowDrawList()->AddText(position, color, text.data(), text.data() + text.size());
    }

    void draw_triangle(ImVec2 center, ImVec2 size, ImColor color, TriangleDirection direction) {
        const ImVec2 half_size = {size.x * 0.5F, size.y * 0.5F};
        ImVec2 first;
        ImVec2 second;
        ImVec2 third;

        switch (direction) {
            case TriangleDirection::Up:
                first = {center.x - half_size.x, center.y + half_size.y};
                second = {center.x, center.y - half_size.y};
                third = {center.x + half_size.x, center.y + half_size.y};
                break;
            case TriangleDirection::Down:
                first = {center.x - half_size.x, center.y - half_size.y};
                second = {center.x + half_size.x, center.y - half_size.y};
                third = {center.x, center.y + half_size.y};
                break;
            case TriangleDirection::Left:
                first = {center.x + half_size.x, center.y - half_size.y};
                second = {center.x + half_size.x, center.y + half_size.y};
                third = {center.x - half_size.x, center.y};
                break;
            case TriangleDirection::Right:
                first = {center.x - half_size.x, center.y - half_size.y};
                second = {center.x - half_size.x, center.y + half_size.y};
                third = {center.x + half_size.x, center.y};
                break;
        }

        ImGui::GetWindowDrawList()->AddTriangleFilled(first, second, third, color);
    }

    void draw_frame(ImVec2 minimum, ImVec2 maximum, ImColor background, ImColor border, float rounding, float border_thickness) {
        ImDrawList& draw_list = *ImGui::GetWindowDrawList();

        draw_list.AddRectFilled(minimum, maximum, background, rounding);
        if (border_thickness <= 0.0F) {
            return;
        }

        const float inset = border_thickness * 0.5F;
        draw_list.AddRect(
            {minimum.x + inset, minimum.y + inset}, {maximum.x - inset, maximum.y - inset}, border, rounding,
            ImDrawFlags_RoundCornersAll, border_thickness
        );
    }

    void draw_frame(Rect rect, const Style& style) {
        draw_frame(rect, style, style.background_color().value);
    }

    void draw_frame(Rect rect, const Style& style, ImColor background) {
        if (style.border() == BORDER_NONE) {
            ImGui::GetWindowDrawList()->AddRectFilled(rect.min, rect.max, background, style.border_radius());
            return;
        }

        draw_frame(
            rect.min, rect.max, background, style.border_color().get_col(), style.border_radius(), style.border_thickness()
        );
    }

    void draw_border(Rect rect, const Style& style) {
        const uint8_t border = style.border();
        if (border == BORDER_NONE || style.border_thickness() <= 0.0F) {
            return;
        }

        const ImU32 color = style.border_color().get_col();
        const float thickness = style.border_thickness();
        if ((border & BORDER_ALL) != 0) {
            ImGui::GetWindowDrawList()->AddRect(
                rect.min, rect.max, color, style.border_radius(), ImDrawFlags_RoundCornersAll, thickness
            );
            return;
        }

        if ((border & BORDER_TOP) != 0) draw_line(rect.min, {rect.max.x, rect.min.y}, color, thickness);
        if ((border & BORDER_BOTTOM) != 0) draw_line({rect.min.x, rect.max.y}, rect.max, color, thickness);
        if ((border & BORDER_LEFT) != 0) draw_line(rect.min, {rect.min.x, rect.max.y}, color, thickness);
        if ((border & BORDER_RIGHT) != 0) draw_line({rect.max.x, rect.min.y}, rect.max, color, thickness);
    }
} // namespace ui
