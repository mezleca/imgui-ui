#include "container.hpp"

#include "../constants.hpp"
#include "../imgui/draw.hpp"
#include "../imgui/effects/blur/blur.hpp"
#include "../imgui/effects/shadow/shadow.hpp"

#include <algorithm>
#include <cmath>
#include <imgui_internal.h>

using namespace ui;

static ImVec4 current_clip(const ImDrawList& draw_list) {
    const ImVec2 min = draw_list.GetClipRectMin();
    const ImVec2 max = draw_list.GetClipRectMax();
    return {min.x, min.y, max.x, max.y};
}

static ImVec4 content_clip(const ComputedStyle& style, Rect rect) {
    const float thickness = style.border_thickness();
    if ((style.border() & BORDER_LEFT) != 0) rect.min.x += thickness;
    if ((style.border() & BORDER_TOP) != 0) rect.min.y += thickness;
    if ((style.border() & BORDER_RIGHT) != 0) rect.max.x -= thickness + 1.0F;
    if ((style.border() & BORDER_BOTTOM) != 0) rect.max.y -= thickness + 1.0F;
    return {rect.min.x, rect.min.y, rect.max.x, rect.max.y};
}

static ImVec4 intersect_clip(ImVec4 clip, Rect rect) {
    clip.x = std::max(clip.x, rect.min.x);
    clip.y = std::max(clip.y, rect.min.y);
    clip.z = std::min(clip.z, rect.max.x);
    clip.w = std::min(clip.w, rect.max.y);
    return clip;
}

bool Container::paint() {
    const ComputedStyle& current_style = computed_style();

    ImGuiChildFlags child_flags = ImGuiChildFlags_AlwaysUseWindowPadding;
    ImGuiWindowFlags window_flags = child_window_flags() | ImGuiWindowFlags_NoBackground;
    const bool scrollable = (m_scroll_vertical || m_scroll_horizontal) && current_style.overflow() != Overflow::Clip;
    if (scrollable) {
        window_flags &= ~(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    }
    if (scrollable && m_scroll_horizontal) {
        window_flags |= ImGuiWindowFlags_HorizontalScrollbar;
    }

    const LayoutSize& size_spec = layout().size_spec();
    if (size_spec.width.mode == LayoutSizeMode::Fit) child_flags |= ImGuiChildFlags_AutoResizeX;
    if (size_spec.height.mode == LayoutSizeMode::Fit) child_flags |= ImGuiChildFlags_AutoResizeY;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, child_window_padding());
    ImGui::SetNextWindowContentSize(child_window_content_size());

    // preserve the allocated max edge after rounding the child origin.
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const ImVec2 child_position = {std::round(position.x), std::round(position.y)};
    const ImVec2 requested_size = child_window_size();
    ImVec2 child_size = {
        std::max(0.0F, requested_size.x + position.x - child_position.x),
        std::max(0.0F, requested_size.y + position.y - child_position.y),
    };
    // keep a growing child inside the parent width so its scrollbar remains hittable.
    if (size_spec.width.mode == LayoutSizeMode::Grow) {
        child_size.x = std::min(child_size.x, ImGui::GetCurrentWindow()->InnerRect.GetWidth());
    }
    ImGui::SetCursorScreenPos(child_position);

    const ImGuiID child_id = id().empty() ? ImGui::GetID(this) : ImGui::GetID(id().c_str());
    // beginchild replaces the incoming clip before border and overflow drawing.
    const ImVec4 parent_clip = current_clip(*ImGui::GetWindowDrawList());
    const ImVec4 parent_effect_clip = current_effect_clip(parent_clip);
    ImGui::BeginChild(child_id, child_size, child_flags, window_flags);

    const Rect resolved_child_rect = Rect::from_position_size(ImGui::GetWindowPos(), ImGui::GetWindowSize());
    set_layout_rect(resolved_child_rect);
    set_visual_rect(resolved_child_rect);
    ImDrawList* child_draw_list = ImGui::GetWindowDrawList();
    // effects are the only draws that need the ancestor effect clip.
    if (EffectRegistry* effects = effect_registry();
        effects != nullptr && (current_style.blur() > 0 || current_style.box_shadow().color.Value.w > 0.0F)) {
        const ImRect blur_rect = ImGui::GetCurrentWindow()->InnerRect;
        ImGui::PushClipRect({parent_effect_clip.x, parent_effect_clip.y}, {parent_effect_clip.z, parent_effect_clip.w}, false);
        const float paint_opacity = std::clamp(ImGui::GetStyle().Alpha, 0.0F, 1.0F);
        draw_blur(
            *effects, *child_draw_list, {blur_rect.Min, blur_rect.Max}, current_style.blur(), current_style.border_radius(),
            paint_opacity
        );
        draw_box_shadow(
            *effects, *child_draw_list, shadow_rect(resolved_child_rect), current_style.box_shadow(),
            current_style.border_radius(), paint_opacity
        );
        ImGui::PopClipRect();
    }

    // draw the frame first so descendant surfaces cannot cover its border.
    ImGui::PushClipRect({parent_clip.x, parent_clip.y}, {parent_clip.z, parent_clip.w}, true);
    draw_frame(*child_draw_list, resolved_child_rect, current_style);
    ImGui::PopClipRect();

    // visible overflow escapes this box but stays inside ancestor effect clips.
    push_effect_clip(
        current_style.overflow() == Overflow::Visible ? parent_effect_clip
                                                      : intersect_clip(parent_effect_clip, resolved_child_rect)
    );

    // child window clipping does not protect the border from descendant drawing.
    m_content_clip_pushed = !scrollable || current_style.border() != BORDER_NONE;
    if (m_content_clip_pushed) {
        if (current_style.overflow() == Overflow::Visible && current_style.border() == BORDER_NONE) {
            ImGui::PushClipRect({parent_clip.x, parent_clip.y}, {parent_clip.z, parent_clip.w}, false);
        } else {
            const ImVec4 clip = content_clip(current_style, resolved_child_rect);
            ImGui::PushClipRect({clip.x, clip.y}, {clip.z, clip.w}, true);
        }
    }

    ImGui::PopStyleVar();
    return true;
}

void Container::on_draw_end() {
    // save final bounds before input registration and deferred decorations read them.
    ImGuiWindow* child_window = ImGui::GetCurrentWindow();
    const Rect child_rect = Rect::from_position_size(ImGui::GetWindowPos(), ImGui::GetWindowSize());
    set_layout_rect(child_rect);
    set_visual_rect(child_rect);

    if (m_content_clip_pushed) {
        ImGui::PopClipRect();
        m_content_clip_pushed = false;
    }
    // clear the parent wheel lock when the hovered window is a nested scrollable child.
    if ((m_scroll_vertical || m_scroll_horizontal) && GImGui->WheelingWindow != nullptr &&
        GImGui->WheelingWindow != child_window && GImGui->HoveredWindow != nullptr &&
        ImGui::IsWindowChildOf(GImGui->HoveredWindow, child_window, false)) {
        GImGui->WheelingWindow = nullptr;
        ImGui::SetKeyOwner(ImGuiKey_MouseWheelX, ImGuiKeyOwner_NoOwner);
        ImGui::SetKeyOwner(ImGuiKey_MouseWheelY, ImGuiKeyOwner_NoOwner);
    }
    ImGui::EndChild();
    pop_effect_clip();
}
