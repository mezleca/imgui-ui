#include "draw.hpp"

#include "blur.hpp"

#include "../style/style.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <numbers>

using namespace ui;

static constexpr float PI = std::numbers::pi_v<float>;
static constexpr float QUARTER_PI = PI * 0.25F;
static constexpr float HALF_PI = PI * 0.5F;
static constexpr float ARC_MAX_ERROR = 0.25F; // maximum sagitta error, matching imgui adaptive circle tessellation model.

struct BorderEntry {
    Rect rect;
    float rounding = 0.0F;
    BorderPath path;
    bool valid = false;
};

static bool is_selected(const BorderPathSegment& segment, uint8_t border) {
    return (segment.sides & border) != 0;
}

static ImVec2 point_at(const BorderPathSegment& segment, float distance) {
    const float progress = segment.length > 0.0F ? std::clamp(distance / segment.length, 0.0F, 1.0F) : 0.0F;
    if (segment.type == BorderPathSegmentType::Line) {
        return {
            std::lerp(segment.start.x, segment.end.x, progress),
            std::lerp(segment.start.y, segment.end.y, progress),
        };
    }

    // distance is normalized by arc length, keeping dash and dot spacing uniform around corners.
    const float angle = std::lerp(segment.start_angle, segment.end_angle, progress);
    const float radius = segment.length / std::abs(segment.end_angle - segment.start_angle);
    return {segment.center.x + std::cos(angle) * radius, segment.center.y + std::sin(angle) * radius};
}

static float arc_max_step(const BorderPathSegment& segment) {
    const float sweep = std::abs(segment.end_angle - segment.start_angle);
    const float radius = segment.length / sweep;
    return radius <= ARC_MAX_ERROR ? sweep : 2.0F * std::acos(std::clamp(1.0F - ARC_MAX_ERROR / radius, -1.0F, 1.0F));
}

static void
append_segment_range(ImDrawList& draw_list, const BorderPathSegment& segment, float start, float end, float max_step = 0.0F) {
    if (end <= start) {
        return;
    }

    if (segment.type == BorderPathSegmentType::Line) {
        draw_list.PathLineToMergeDuplicate(point_at(segment, end));
        return;
    }

    // for a chord angle theta, sagitta = radius * (1 - cos(theta / 2)).
    // solving it for theta yields the largest step below the configured arc error.
    const float sweep = std::abs(segment.end_angle - segment.start_angle);
    if (max_step <= 0.0F) {
        max_step = arc_max_step(segment);
    }

    const int steps = std::max(1, static_cast<int>(std::ceil(sweep * (end - start) / segment.length / max_step)));
    for (int step = 1; step <= steps; ++step) {
        const float distance = std::lerp(start, end, static_cast<float>(step) / static_cast<float>(steps));
        draw_list.PathLineToMergeDuplicate(point_at(segment, distance));
    }
}

static std::size_t first_selected_run_segment(const BorderPath& path, uint8_t border) {
    for (std::size_t index = 0; index < path.segments.size(); ++index) {
        const BorderPathSegment& segment = path.segments[index];
        if (segment.length > 0.0F && !is_selected(segment, border)) {
            return (index + 1) % path.segments.size();
        }
    }
    return 0;
}

template <typename OnSegment, typename OnRunBreak>
static void walk_selected_segments(const BorderPath& path, uint8_t border, OnSegment&& on_segment, OnRunBreak&& on_run_break) {
    // start after an unselected segment so a selected run is never split by the path seam.
    const std::size_t first = first_selected_run_segment(path, border);

    for (std::size_t offset = 0; offset < path.segments.size(); ++offset) {
        const BorderPathSegment& segment = path.segments[(first + offset) % path.segments.size()];
        if (segment.length <= 0.0F) {
            continue;
        }

        if (!is_selected(segment, border)) {
            on_run_break();
            continue;
        }

        on_segment(segment);
    }

    on_run_break();
}

static void stroke_solid_path(ImDrawList& draw_list, const BorderPath& path, uint8_t border, ImU32 color, float thickness) {
    bool has_path = false;

    walk_selected_segments(
        path, border,
        [&](const BorderPathSegment& segment) {
            if (!has_path) {
                draw_list.PathLineTo(segment.start);
                has_path = true;
            }
            append_segment_range(draw_list, segment, 0.0F, segment.length);
        },
        [&]() {
            if (has_path) {
                draw_list.PathStroke(color, thickness);
                has_path = false;
            }
        }
    );
}

static void stroke_dashed_path(ImDrawList& draw_list, const BorderPath& path, uint8_t border, ImU32 color, float thickness) {
    const float dash_length = std::max(6.0F, thickness * 4.0F);
    const float gap_length = std::max(3.0F, thickness * 2.0F);

    float remaining = dash_length;
    bool drawing = true;
    bool has_path = false;

    const auto flush = [&]() {
        if (has_path) {
            draw_list.PathStroke(color, thickness);
            has_path = false;
        }
    };

    walk_selected_segments(
        path, border,
        [&](const BorderPathSegment& segment) {
            const float max_step = segment.type == BorderPathSegmentType::Arc ? arc_max_step(segment) : 0.0F;
            float distance = 0.0F;
            while (distance < segment.length) {
                const float length = std::min(remaining, segment.length - distance);

                if (drawing) {
                    if (!has_path) {
                        draw_list.PathLineTo(point_at(segment, distance));
                        has_path = true;
                    }
                    append_segment_range(draw_list, segment, distance, distance + length, max_step);
                }

                distance += length;
                remaining -= length;

                if (remaining <= 0.0001F) {
                    drawing = !drawing;
                    remaining = drawing ? dash_length : gap_length;
                    if (!drawing) {
                        flush();
                    }
                }
            }
        },
        [&]() {
            flush();
            // a broken run always restarts on a fresh dash
            remaining = dash_length;
            drawing = true;
        }
    );
}

static void stroke_dotted_path(ImDrawList& draw_list, const BorderPath& path, uint8_t border, ImU32 color, float thickness) {
    const float radius = std::max(0.5F, thickness * 0.5F);
    const float spacing = std::max(3.0F, thickness * 3.0F);
    const int segments = std::clamp(static_cast<int>(std::ceil(std::numbers::pi_v<float> * radius)), 8, 512);
    float distance = spacing * 0.5F;

    walk_selected_segments(
        path, border,
        [&](const BorderPathSegment& segment) {
            while (distance < segment.length) {
                draw_list.AddCircleFilled(point_at(segment, distance), radius, color, segments);
                distance += spacing;
            }
            distance -= segment.length;
        },
        [&]() {
            // a broken run always restarts half a spacing in, same as the very first run
            distance = spacing * 0.5F;
        }
    );
}

void ui::draw_line(ImDrawList& draw_list, ImVec2 start, ImVec2 end, ImColor color, float thickness) {
    draw_list.AddLine(start, end, color, thickness);
}

void ui::draw_line(ImVec2 start, ImVec2 end, ImColor color, float thickness) {
    draw_line(*ImGui::GetWindowDrawList(), start, end, color, thickness);
}

void ui::draw_circle(ImVec2 center, float radius, ImColor color) {
    ImGui::GetWindowDrawList()->AddCircleFilled(center, radius, color);
}

void ui::draw_circle_outline(ImVec2 center, float radius, ImColor color, float thickness) {
    ImGui::GetWindowDrawList()->AddCircle(center, radius, color, 0, thickness);
}

void ui::draw_text(ImVec2 position, ImColor color, std::string_view text) {
    ImGui::GetWindowDrawList()->AddText(position, color, text.data(), text.data() + text.size());
}

void ui::draw_triangle(ImVec2 center, ImVec2 size, ImColor color, TriangleDirection direction) {
    static constexpr std::array<std::array<ImVec2, 3>, 4> DIRECTION_OFFSETS = {{
        {{{-1.0F, 1.0F}, {0.0F, -1.0F}, {1.0F, 1.0F}}},  // up
        {{{-1.0F, -1.0F}, {1.0F, -1.0F}, {0.0F, 1.0F}}}, // down
        {{{1.0F, -1.0F}, {1.0F, 1.0F}, {-1.0F, 0.0F}}},  // left
        {{{-1.0F, -1.0F}, {-1.0F, 1.0F}, {1.0F, 0.0F}}}, // right
    }};

    const ImVec2 half_size = {size.x * 0.5F, size.y * 0.5F};
    const auto& offsets = DIRECTION_OFFSETS[static_cast<std::size_t>(direction)];
    const auto vertex = [&](std::size_t index) {
        return ImVec2{center.x + offsets[index].x * half_size.x, center.y + offsets[index].y * half_size.y};
    };

    ImGui::GetWindowDrawList()->AddTriangleFilled(vertex(0), vertex(1), vertex(2), color);
}

static void draw_full_frame(Rect rect, const Style& style, ImColor background, ImColor border) {
    ImDrawList& draw_list = *ImGui::GetWindowDrawList();
    const float border_thickness = style.border_thickness();

    if (border_thickness <= 0.0F) {
        draw_list.AddRectFilled(rect.min, rect.max, background, style.border_radius());
        return;
    }

    const float inset = border_thickness * 0.5F;

    draw_list.AddRectFilled(
        {rect.min.x + border_thickness, rect.min.y + border_thickness},
        {rect.max.x - border_thickness, rect.max.y - border_thickness}, background,
        std::max(0.0F, style.border_radius() - border_thickness)
    );

    draw_list.AddRect(
        {rect.min.x + inset, rect.min.y + inset}, {rect.max.x - inset, rect.max.y - inset}, border, style.border_radius(),
        ImDrawFlags_RoundCornersAll, border_thickness
    );
}

void ui::draw_frame(Rect rect, const Style& style) {
    draw_frame(rect, style, 1.0F);
}

void ui::draw_frame(Rect rect, const Style& style, ImColor background) {
    const float alpha = style.alpha();
    background.Value.w *= alpha;

    ImColor border = style.border_color().value;
    border.Value.w *= alpha;

    draw_blur(rect, style.blur(), style.border_radius(), alpha);
    if (style.border() == BORDER_ALL && style.border_style() == BorderStyle::Solid) {
        draw_full_frame(rect, style, background, border);
        return;
    }

    ImGui::GetWindowDrawList()->AddRectFilled(rect.min, rect.max, background, style.border_radius());
    draw_border(rect, style, border);
}

void ui::draw_frame(Rect rect, const Style& style, float opacity) {
    ImColor background = style.background_color().value;
    ImColor border = style.border_color().value;
    const float alpha = std::clamp(opacity * style.alpha(), 0.0F, 1.0F);
    background.Value.w *= alpha;
    border.Value.w *= alpha;

    draw_blur(rect, style.blur(), style.border_radius(), alpha);
    if (style.border() == BORDER_ALL && style.border_style() == BorderStyle::Solid) {
        draw_full_frame(rect, style, background, border);
        return;
    }

    ImGui::GetWindowDrawList()->AddRectFilled(rect.min, rect.max, background, style.border_radius());
    draw_border(rect, style, border);
}

static BorderPathSegment line(ImVec2 start, ImVec2 end, uint8_t sides) {
    return {BorderPathSegmentType::Line, start, end, {}, 0.0F, 0.0F, std::hypot(end.x - start.x, end.y - start.y), sides};
}

static BorderPathSegment arc(ImVec2 center, float radius, float start_angle, float end_angle, uint8_t sides) {
    const auto point = [center, radius](float angle) {
        return ImVec2{center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius};
    };
    return {
        BorderPathSegmentType::Arc,
        point(start_angle),
        point(end_angle),
        center,
        start_angle,
        end_angle,
        std::abs(end_angle - start_angle) * radius,
        sides
    };
}

BorderPath ui::rounded_rect_border_path(Rect rect, float rounding) {
    const ImVec2 size = rect.size();
    const float width = std::max(0.0F, size.x);
    const float height = std::max(0.0F, size.y);
    const float radius = std::min({std::max(0.0F, rounding), width * 0.5F, height * 0.5F});

    const ImVec2 top_left = {rect.min.x + radius, rect.min.y + radius};
    const ImVec2 top_right = {rect.max.x - radius, rect.min.y + radius};
    const ImVec2 bottom_right = {rect.max.x - radius, rect.max.y - radius};
    const ImVec2 bottom_left = {rect.min.x + radius, rect.max.y - radius};

    BorderPath path = {.segments{
        {line({top_left.x, rect.min.y}, {top_right.x, rect.min.y}, BORDER_TOP),
         arc(top_right, radius, -HALF_PI, -QUARTER_PI, BORDER_TOP), arc(top_right, radius, -QUARTER_PI, 0.0F, BORDER_RIGHT),
         line({rect.max.x, top_right.y}, {rect.max.x, bottom_right.y}, BORDER_RIGHT),
         arc(bottom_right, radius, 0.0F, QUARTER_PI, BORDER_RIGHT), arc(bottom_right, radius, QUARTER_PI, HALF_PI, BORDER_BOTTOM),
         line({bottom_right.x, rect.max.y}, {bottom_left.x, rect.max.y}, BORDER_BOTTOM),
         arc(bottom_left, radius, HALF_PI, QUARTER_PI * 3.0F, BORDER_BOTTOM),
         arc(bottom_left, radius, QUARTER_PI * 3.0F, PI, BORDER_LEFT),
         line({rect.min.x, bottom_left.y}, {rect.min.x, top_left.y}, BORDER_LEFT),
         arc(top_left, radius, PI, QUARTER_PI * 5.0F, BORDER_LEFT),
         arc(top_left, radius, QUARTER_PI * 5.0F, PI + HALF_PI, BORDER_TOP)}
    }};

    return path;
}

static const BorderPath& border_path(Rect rect, float rounding) {
    static std::array<BorderEntry, 16> paths;

    const uint32_t min_x = std::bit_cast<uint32_t>(rect.min.x);
    const uint32_t min_y = std::bit_cast<uint32_t>(rect.min.y);
    const uint32_t max_x = std::bit_cast<uint32_t>(rect.max.x);
    const uint32_t max_y = std::bit_cast<uint32_t>(rect.max.y);
    const uint32_t rounded = std::bit_cast<uint32_t>(rounding);
    const std::size_t index = (min_x ^ (min_y << 3) ^ (max_x << 7) ^ (max_y << 11) ^ (rounded << 13)) % paths.size();

    BorderEntry& entry = paths[index];

    if (entry.valid && entry.rect.min.x == rect.min.x && entry.rect.min.y == rect.min.y && entry.rect.max.x == rect.max.x &&
        entry.rect.max.y == rect.max.y && entry.rounding == rounding) {
        return entry.path;
    }

    entry.rect = rect;
    entry.rounding = rounding;
    entry.path = rounded_rect_border_path(rect, rounding);
    entry.valid = true;

    return entry.path;
}

void ui::draw_border_path(
    ImDrawList& draw_list, const BorderPath& path, uint8_t border, ImColor color, float thickness, BorderStyle style
) {
    if (border == BORDER_NONE || thickness <= 0.0F) {
        return;
    }

    const ImU32 draw_color = color;
    switch (style) {
        case BorderStyle::Solid:
            stroke_solid_path(draw_list, path, border, draw_color, thickness);
            return;
        case BorderStyle::Dashed:
            stroke_dashed_path(draw_list, path, border, draw_color, thickness);
            return;
        case BorderStyle::Dotted:
            stroke_dotted_path(draw_list, path, border, draw_color, thickness);
            return;
    }
}

void ui::draw_border_path(const BorderPath& path, uint8_t border, ImColor color, float thickness, BorderStyle style) {
    draw_border_path(*ImGui::GetWindowDrawList(), path, border, color, thickness, style);
}

void ui::draw_border(Rect rect, const Style& style) {
    draw_border(rect, style, style.border_color().value);
}

void ui::draw_border(Rect rect, const Style& style, ImColor color) {
    draw_border(*ImGui::GetWindowDrawList(), rect, style, color);
}

void ui::draw_border(ImDrawList& draw_list, Rect rect, const Style& style, ImColor color) {
    if (style.border() == BORDER_ALL && style.border_style() == BorderStyle::Solid) {
        draw_list.AddRect(rect.min, rect.max, color, style.border_radius(), style.border_thickness());
        return;
    }

    draw_border_path(
        draw_list, border_path(rect, style.border_radius()), style.border(), color, style.border_thickness(), style.border_style()
    );
}
