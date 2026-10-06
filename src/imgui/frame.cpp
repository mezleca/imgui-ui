#include <imgui-ui/imgui/draw.hpp>
#include <imgui-ui/imgui/paint-state.hpp>
#include <imgui-ui/imgui/effects/blur/blur.hpp>
#include <imgui-ui/imgui/effects/gradient/gradient.hpp>
#include <imgui-ui/imgui/effects/shadow/shadow.hpp>

#include <algorithm>

using namespace ui;

static void draw_full_frame(ImDrawList& draw_list, Rect rect, const ComputedStyle& style, ImColor background, ImColor border) {
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

void Painter::frame_surface(Rect rect, const ComputedStyle& style, const std::optional<Color>& background_override) const {
    const Color& background_color = background_override ? *background_override : style.background_color().value;
    ImColor background = color(background_color);
    const Color& border_color = style.border_color().value;
    const ImColor draw_border_color = color(border_color);

    if (m_effects != nullptr && background_color.gradient() != nullptr &&
        draw_gradient_rect(*m_effects, m_draw_list, rect, background_color, style.border_radius(), m_alpha)) {
        background.Value.w = 0.0F;
    }

    const bool gradient_border = m_effects != nullptr && border_color.gradient() != nullptr && style.border() == BORDER_ALL &&
                                 style.border_style() == BorderStyle::Solid && style.border_thickness() > 0.0F;
    if (style.border() == BORDER_ALL && style.border_style() == BorderStyle::Solid) {
        ImColor frame_border = draw_border_color;
        if (gradient_border) frame_border.Value.w = 0.0F;

        draw_full_frame(m_draw_list, rect, style, background, frame_border);
        if (gradient_border &&
            !draw_gradient_rect(
                *m_effects, m_draw_list, rect, border_color, style.border_radius(), m_alpha, style.border_thickness()
            )) {
            border(rect, style, border_color);
        }
        return;
    }

    m_draw_list.AddRectFilled(rect.min, rect.max, background, style.border_radius());
    border(rect, style, border_color);
}

void Painter::frame_effects(Rect rect, const ComputedStyle& style) const {
    if (m_effects == nullptr) return;

    const ImVec2 clip_min = m_draw_list.GetClipRectMin();
    const ImVec2 clip_max = m_draw_list.GetClipRectMax();
    // effects inherit the ancestor clip before content painting restores the draw list's own clip.
    if (m_state != nullptr) {
        const ImVec4 clip = m_state->effect_clip({clip_min.x, clip_min.y, clip_max.x, clip_max.y});
        m_draw_list.PushClipRect({clip.x, clip.y}, {clip.z, clip.w}, false);
    }

    shadow(rect, style);
    blur(rect, style);

    if (m_state != nullptr) m_draw_list.PopClipRect();
}

void Painter::blur(Rect rect, const ComputedStyle& style) const {
    if (m_effects != nullptr && style.blur() > 0) {
        draw_blur(*m_effects, m_draw_list, rect, style.blur(), style.border_radius(), m_alpha);
    }
}

void Painter::shadow(Rect rect, const ComputedStyle& style) const {
    if (m_effects != nullptr && style.box_shadow().color.max_alpha() > 0.0F) {
        draw_box_shadow(*m_effects, m_draw_list, rect, style.box_shadow(), style.border_radius(), m_alpha);
    }
}

void Painter::frame(Rect rect, const ComputedStyle& style, const std::optional<Color>& background) const {
    frame_effects(rect, style);
    frame_surface(rect, style, background);
}

void ui::draw_frame(
    ImDrawList& draw_list, Rect rect, const ComputedStyle& style, EffectRegistry* effects, float opacity,
    const std::optional<Color>& background
) {
    Painter(draw_list, nullptr, effects, opacity).frame(rect, style, background);
}

void ui::draw_frame_surface(ImDrawList& draw_list, Rect rect, const ComputedStyle& style, EffectRegistry* effects) {
    Painter(draw_list, nullptr, effects).frame_surface(rect, style);
}
