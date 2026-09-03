#include "draw.hpp"

#include "effects/blur/blur.hpp"
#include "effects/shadow/shadow.hpp"

#include "../style/style.hpp"
#include "../widgets/text-value.hpp"

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

template <typename OnSegment>
static bool walk_side_segments(const BorderPath& path, uint8_t side, OnSegment&& on_segment) {
    // visits one side, including the two half-corner arcs assigned to it.
    const std::size_t first = first_selected_run_segment(path, side);
    bool started = false;

    for (std::size_t offset = 0; offset < path.segments.size(); ++offset) {
        const BorderPathSegment& segment = path.segments[(first + offset) % path.segments.size()];
        if (segment.length <= 0.0F) {
            continue;
        }

        if (!is_selected(segment, side)) {
            if (started) {
                break;
            }
            continue;
        }

        started = true;
        on_segment(segment);
    }

    return started;
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

static constexpr uint8_t BORDER_SIDES[] = {BORDER_TOP, BORDER_RIGHT, BORDER_BOTTOM, BORDER_LEFT};

static void stroke_dashed_side(ImDrawList& draw_list, const BorderPath& path, uint8_t side, ImU32 color, float thickness) {
    // fit complete dash and gap periods to the side so both ends have equal spacing.
    const float preferred_dash = std::max(6.0F, thickness * 4.0F);
    const float preferred_gap = std::max(3.0F, thickness * 2.0F);
    const float preferred_period = preferred_dash + preferred_gap;
    float side_length = 0.0F;
    if (!walk_side_segments(path, side, [&](const BorderPathSegment& segment) { side_length += segment.length; })) {
        return;
    }

    const int dash_count = std::max(1, static_cast<int>(std::floor(side_length / preferred_period)));
    const float period = side_length / static_cast<float>(dash_count);
    if (dash_count == 1 && side_length <= preferred_dash) {
        stroke_solid_path(draw_list, path, side, color, thickness);
        return;
    }

    const float dash_length = std::min(preferred_dash, period);
    const float gap_length = period - dash_length;
    float remaining = gap_length * 0.5F;
    bool drawing = false;
    bool has_path = false;

    const auto flush = [&]() {
        if (has_path) {
            draw_list.PathStroke(color, thickness);
            has_path = false;
        }
    };

    walk_side_segments(path, side, [&](const BorderPathSegment& segment) {
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
    });
    flush();
}

static void stroke_dashed_path(ImDrawList& draw_list, const BorderPath& path, uint8_t border, ImU32 color, float thickness) {
    for (const uint8_t side : BORDER_SIDES) {
        if ((border & side) != 0) {
            stroke_dashed_side(draw_list, path, side, color, thickness);
        }
    }
}

static void stroke_dotted_side(ImDrawList& draw_list, const BorderPath& path, uint8_t side, ImU32 color, float thickness) {
    // center dots over the side instead of restarting the pattern at each path segment.
    const float radius = std::max(0.5F, thickness * 0.5F);
    const float spacing = std::max(3.0F, thickness * 3.0F);
    static const auto unit_circle = [] {
        std::array<ImVec2, 24> points{};
        for (std::size_t index = 0; index < points.size(); ++index) {
            const float angle = std::numbers::pi_v<float> * 2.0F * static_cast<float>(index) / static_cast<float>(points.size());
            points[index] = {std::cos(angle), std::sin(angle)};
        }
        return points;
    }();

    const int segments = radius <= 1.5F ? 8 : radius <= 2.5F ? 12 : radius <= 5.0F ? 12 : 24;
    const int stride = static_cast<int>(unit_circle.size()) / segments;
    std::array<ImVec2, 24> points{};
    const auto draw_dot = [&](ImVec2 center) {
        if (radius <= 0.75F) {
            draw_list.AddRectFilled({center.x - radius, center.y - radius}, {center.x + radius, center.y + radius}, color);
            return;
        }

        for (int index = 0; index < segments; ++index) {
            const ImVec2 unit = unit_circle[static_cast<std::size_t>(index * stride)];
            points[static_cast<std::size_t>(index)] = {center.x + unit.x * radius, center.y + unit.y * radius};
        }
        draw_list.AddConvexPolyFilled(points.data(), segments, color);
    };

    std::array<const BorderPathSegment*, 12> run{};
    std::size_t run_count = 0;
    float run_length = 0.0F;
    if (!walk_side_segments(path, side, [&](const BorderPathSegment& segment) {
            run[run_count++] = &segment;
            run_length += segment.length;
        })) {
        return;
    }

    const int dot_count = std::max(1, static_cast<int>(std::floor(run_length / spacing)));
    const float pitch = run_length / static_cast<float>(dot_count);
    for (int dot = 0; dot < dot_count; ++dot) {
        float distance = pitch * (static_cast<float>(dot) + 0.5F);
        for (std::size_t index = 0; index < run_count; ++index) {
            const BorderPathSegment& segment = *run[index];
            if (distance <= segment.length || index + 1 == run_count) {
                draw_dot(point_at(segment, distance));
                break;
            }
            distance -= segment.length;
        }
    }
}

static void stroke_dotted_path(ImDrawList& draw_list, const BorderPath& path, uint8_t border, ImU32 color, float thickness) {
    for (const uint8_t side : BORDER_SIDES) {
        if ((border & side) != 0) {
            stroke_dotted_side(draw_list, path, side, color, thickness);
        }
    }
}

ImDrawList& ui::draw_list(DrawListTarget target) {
    switch (target) {
        case DrawListTarget::Background:
            return *ImGui::GetBackgroundDrawList();
        case DrawListTarget::Foreground:
            return *ImGui::GetForegroundDrawList();
        case DrawListTarget::Window:
            return *ImGui::GetWindowDrawList();
    }

    return *ImGui::GetWindowDrawList();
}

void ui::draw_line(ImDrawList& draw_list, ImVec2 start, ImVec2 end, ImColor color, float thickness) {
    draw_list.AddLine(start, end, color, thickness);
}

void ui::draw_line(ImVec2 start, ImVec2 end, ImColor color, float thickness, DrawListTarget target) {
    draw_line(ui::draw_list(target), start, end, color, thickness);
}

void ui::draw_circle(ImDrawList& draw_list, ImVec2 center, float radius, ImColor color) {
    draw_list.AddCircleFilled(center, radius, color);
}

void ui::draw_circle(ImVec2 center, float radius, ImColor color, DrawListTarget target) {
    draw_circle(draw_list(target), center, radius, color);
}

void ui::draw_circle_outline(ImDrawList& draw_list, ImVec2 center, float radius, ImColor color, float thickness) {
    draw_list.AddCircle(center, radius, color, 0, thickness);
}

void ui::draw_circle_outline(ImVec2 center, float radius, ImColor color, float thickness, DrawListTarget target) {
    draw_circle_outline(draw_list(target), center, radius, color, thickness);
}

void ui::draw_text(ImDrawList& draw_list, ImVec2 position, ImColor color, std::string_view text) {
    draw_list.AddText(position, color, text.data(), text.data() + text.size());
}

void ui::draw_text(ImVec2 position, ImColor color, std::string_view text, DrawListTarget target) {
    draw_text(draw_list(target), position, color, text);
}

void ui::draw_text(ImDrawList& draw_list, ImVec2 position, ImColor color, const GenericValue& text, const ImVec4* clip_rect) {
    ImFont* font = text.font() != nullptr ? text.font() : ImGui::GetFont();
    const float font_size = ImGui::GetFontSize();
    const float wrap_width = std::max(0.0F, text.wrap_width());
    if (text.line_height_multiplier() == 1.0F) {
        draw_list.AddText(font, font_size, position, color, text.c_str(), nullptr, wrap_width, clip_rect);
        return;
    }

    const char* const value = text.c_str();
    const char* const value_end = value + std::char_traits<char>::length(value);
    const float line_height = font_size * text.line_height_multiplier();
    float y = position.y;

    for (const char* paragraph = value;;) {
        const char* const paragraph_end = std::find(paragraph, value_end, '\n');
        const char* line = paragraph;

        do {
            const char* line_end = paragraph_end;
            if (wrap_width > 0.0F && line < paragraph_end) {
                line_end = font->CalcWordWrapPosition(font_size, line, paragraph_end, wrap_width);
                if (line_end == line) {
                    line_end = paragraph_end;
                }
            }

            draw_list.AddText(font, font_size, {position.x, y}, color, line, line_end, 0.0F, clip_rect);
            y += line_height;

            if (line_end == paragraph_end) {
                break;
            }

            line = line_end;
            while (line < paragraph_end && (*line == ' ' || *line == '\t')) {
                ++line;
            }
        } while (line < paragraph_end);

        if (paragraph_end == value_end) {
            break;
        }

        paragraph = paragraph_end + 1;
    }
}

void ui::draw_text(ImVec2 position, ImColor color, const GenericValue& text, const ImVec4* clip_rect, DrawListTarget target) {
    draw_text(draw_list(target), position, color, text, clip_rect);
}

void ui::draw_text_ellipsis(ImDrawList& draw_list, ImVec2 position, ImColor color, const GenericValue& text, ImVec4 clip_rect) {
    ImFont* font = text.font() != nullptr ? text.font() : ImGui::GetFont();
    const float font_size = ImGui::GetFontSize();
    const char* const value = text.c_str();
    const char* const value_end = value + std::char_traits<char>::length(value);
    const float line_height = font_size * text.line_height_multiplier();
    float y = position.y;

    for (const char* line = value;;) {
        const char* const line_end = std::find(line, value_end, '\n');
        const float available_width = std::max(0.0F, clip_rect.z - position.x);
        const ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0F, line, line_end);
        if (text_size.x <= available_width) {
            draw_list.AddText(font, font_size, {position.x, y}, color, line, line_end, 0.0F, &clip_rect);
        } else {
            constexpr std::string_view ellipsis = "...";
            const float ellipsis_width = font->CalcTextSizeA(font_size, FLT_MAX, 0.0F, ellipsis.data()).x;
            const char* visible_end = line;
            const float text_width = std::max(0.0F, available_width - ellipsis_width);
            const ImVec2 visible_size = font->CalcTextSizeA(font_size, text_width, 0.0F, line, line_end, &visible_end);

            if (visible_end != line) {
                draw_list.AddText(font, font_size, {position.x, y}, color, line, visible_end, 0.0F, &clip_rect);
            }

            draw_list.AddText(
                font, font_size, {position.x + visible_size.x, y}, color, ellipsis.data(), nullptr, 0.0F, &clip_rect
            );
        }

        if (line_end == value_end) {
            break;
        }

        line = line_end + 1;
        y += line_height;
    }
}

void ui::draw_text_ellipsis(ImVec2 position, ImColor color, const GenericValue& text, ImVec4 clip_rect, DrawListTarget target) {
    draw_text_ellipsis(draw_list(target), position, color, text, clip_rect);
}

void ui::draw_triangle(ImDrawList& draw_list, ImVec2 center, ImVec2 size, ImColor color, TriangleDirection direction) {
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

    draw_list.AddTriangleFilled(vertex(0), vertex(1), vertex(2), color);
}

void ui::draw_triangle(ImVec2 center, ImVec2 size, ImColor color, TriangleDirection direction, DrawListTarget target) {
    draw_triangle(draw_list(target), center, size, color, direction);
}

static void draw_full_frame(ImDrawList& draw_list, Rect rect, const Style& style, ImColor background, ImColor border) {
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

static void draw_frame_surface_impl(ImDrawList& draw_list, Rect rect, const Style& style, ImColor background, float alpha) {
    background.Value.w *= alpha;

    ImColor border = style.border_color().value;
    border.Value.w *= alpha;

    if (style.border() == BORDER_ALL && style.border_style() == BorderStyle::Solid) {
        draw_full_frame(draw_list, rect, style, background, border);
        return;
    }

    draw_list.AddRectFilled(rect.min, rect.max, background, style.border_radius());
    draw_border(draw_list, rect, style, border);
}

static void draw_frame_impl(ImDrawList& draw_list, Rect rect, const Style& style, ImColor background, float alpha) {
    draw_box_shadow(draw_list, rect, style.box_shadow(), style.border_radius(), alpha);
    draw_blur(draw_list, rect, style.blur(), style.border_radius(), alpha);
    draw_frame_surface_impl(draw_list, rect, style, background, alpha);
}

void ui::draw_frame(ImDrawList& draw_list, Rect rect, const Style& style) {
    draw_frame(draw_list, rect, style, 1.0F);
}

void ui::draw_frame(Rect rect, const Style& style, DrawListTarget target) {
    draw_frame(draw_list(target), rect, style);
}

void ui::draw_frame(ImDrawList& draw_list, Rect rect, const Style& style, ImColor background) {
    draw_frame_impl(draw_list, rect, style, background, style.alpha());
}

void ui::draw_frame(Rect rect, const Style& style, ImColor background, DrawListTarget target) {
    draw_frame(draw_list(target), rect, style, background);
}

void ui::draw_frame(ImDrawList& draw_list, Rect rect, const Style& style, float opacity) {
    const float alpha = std::clamp(opacity * style.alpha(), 0.0F, 1.0F);
    draw_frame_impl(draw_list, rect, style, style.background_color().value, alpha);
}

void ui::draw_frame(Rect rect, const Style& style, float opacity, DrawListTarget target) {
    draw_frame(draw_list(target), rect, style, opacity);
}

void ui::draw_frame_surface(ImDrawList& draw_list, Rect rect, const Style& style, float opacity) {
    const float alpha = std::clamp(opacity * style.alpha(), 0.0F, 1.0F);
    draw_frame_surface_impl(draw_list, rect, style, style.background_color().value, alpha);
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
    static std::array<BorderEntry, 64> paths;

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
    if ((draw_color & IM_COL32_A_MASK) == 0) {
        return;
    }

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
    draw_border_path(draw_list(), path, border, color, thickness, style);
}

void ui::draw_border(Rect rect, const Style& style, DrawListTarget target) {
    draw_border(rect, style, style.border_color().value, target);
}

void ui::draw_border(Rect rect, const Style& style, ImColor color, DrawListTarget target) {
    draw_border(draw_list(target), rect, style, color);
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
