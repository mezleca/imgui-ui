#include <imgui-ui/imgui/draw.hpp>
#include <imgui-ui/imgui/paint-state.hpp>

#include <algorithm>
#include <array>

using namespace ui;

Painter::Painter(ImDrawList& draw_list, PaintState* state, EffectRegistry* effects, float opacity)
    : m_draw_list(draw_list), m_state(state), m_effects(effects),
      m_alpha(std::clamp(ImGui::GetStyle().Alpha, 0.0F, 1.0F) * std::clamp(opacity, 0.0F, 1.0F)) {}

ImColor Painter::color(const Color& source) const {
    ImVec4 rgba = source.rgba();
    rgba.w *= m_alpha;
    return ImColor{rgba};
}

void Painter::line(ImVec2 start, ImVec2 end, const Color& source, float thickness) const {
    m_draw_list.AddLine(start, end, color(source), thickness);
}

void Painter::circle(ImVec2 center, float radius, const Color& source) const {
    m_draw_list.AddCircleFilled(center, radius, color(source));
}

void Painter::circle_outline(ImVec2 center, float radius, const Color& source, float thickness) const {
    m_draw_list.AddCircle(center, radius, color(source), 0, thickness);
}

void Painter::rect_filled(Rect rect, const Color& source, float rounding, ImDrawFlags flags) const {
    m_draw_list.AddRectFilled(rect.min, rect.max, color(source), rounding, flags);
}

void Painter::rect_outline(Rect rect, const Color& source, float thickness, float rounding) const {
    if (thickness <= 0.0F) return;

    m_draw_list.AddRect(rect.min, rect.max, color(source), rounding, ImDrawFlags_RoundCornersAll, std::max(1.0F, thickness));
}

void Painter::triangle(ImVec2 center, ImVec2 size, const Color& source, TriangleDirection direction) const {
    static constexpr std::array<std::array<ImVec2, 3>, 4> DIRECTION_OFFSETS = {
        {
            {{{-1.0F, 1.0F}, {0.0F, -1.0F}, {1.0F, 1.0F}}},
            {{{-1.0F, -1.0F}, {1.0F, -1.0F}, {0.0F, 1.0F}}},
            {{{1.0F, -1.0F}, {1.0F, 1.0F}, {-1.0F, 0.0F}}},
            {{{-1.0F, -1.0F}, {-1.0F, 1.0F}, {1.0F, 0.0F}}},
        },
    };

    const ImVec2 half_size = {size.x * 0.5F, size.y * 0.5F};
    const auto& offsets = DIRECTION_OFFSETS[static_cast<std::size_t>(direction)];
    const auto vertex = [&](std::size_t index) {
        return ImVec2{center.x + (offsets[index].x * half_size.x), center.y + (offsets[index].y * half_size.y)};
    };

    m_draw_list.AddTriangleFilled(vertex(0), vertex(1), vertex(2), color(source));
}

void Painter::image(ImTextureID texture, Rect rect, ImVec2 uv_min, ImVec2 uv_max, const Color& tint, float rounding) const {
    m_draw_list.AddImageRounded(texture, rect.min, rect.max, uv_min, uv_max, color(tint), rounding, ImDrawFlags_RoundCornersAll);
}

void Painter::border(Rect rect, const ComputedStyle& style, const Color& source) const {
    const ImColor draw_color = color(source);
    if (style.border() == BORDER_ALL && style.border_style() == BorderStyle::Solid) {
        m_draw_list.AddRect(rect.min, rect.max, draw_color, style.border_radius(), style.border_thickness());
        return;
    }

    if (m_state != nullptr) {
        draw_border_path(
            m_draw_list, m_state->border_path(rect, style.border_radius()), style.border(), draw_color, style.border_thickness(),
            style.border_style()
        );
        return;
    }

    draw_border_path(
        m_draw_list, rounded_rect_border_path(rect, style.border_radius()), style.border(), draw_color, style.border_thickness(),
        style.border_style()
    );
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

Rect ui::viewport_work_area() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr) return {};

    const ImVec2 size =
        viewport->WorkSize.x > 0.0F && viewport->WorkSize.y > 0.0F ? viewport->WorkSize : ImGui::GetIO().DisplaySize;
    return Rect::from_position_size(viewport->WorkPos, size);
}

void ui::draw_line(ImDrawList& draw_list, ImVec2 start, ImVec2 end, const Color& color, float thickness) {
    Painter(draw_list).line(start, end, color, thickness);
}

void ui::draw_circle(ImDrawList& draw_list, ImVec2 center, float radius, const Color& color) {
    Painter(draw_list).circle(center, radius, color);
}

void ui::draw_circle_outline(ImDrawList& draw_list, ImVec2 center, float radius, const Color& color, float thickness) {
    Painter(draw_list).circle_outline(center, radius, color, thickness);
}

void ui::draw_rect_filled(ImDrawList& draw_list, Rect rect, const Color& color, float rounding, ImDrawFlags flags) {
    Painter(draw_list).rect_filled(rect, color, rounding, flags);
}

void ui::draw_rect_outline(ImDrawList& draw_list, Rect rect, const Color& color, float thickness, float rounding) {
    Painter(draw_list).rect_outline(rect, color, thickness, rounding);
}

void ui::draw_triangle(ImDrawList& draw_list, ImVec2 center, ImVec2 size, const Color& color, TriangleDirection direction) {
    Painter(draw_list).triangle(center, size, color, direction);
}
