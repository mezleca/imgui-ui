#include "container.hpp"
#include "geometry.hpp"
#include "../constants.hpp"
#include "../imgui/draw.hpp"
#include "../imgui/effects/blur/blur.hpp"
#include "../imgui/effects/shadow/shadow.hpp"
#include <algorithm>
#include <cmath>
#include <utility>

using namespace ui;

Container::Container(std::string id, std::string_view type_name) : Widget(std::move(id), type_name, InputMode::None) {
    configure_all_styles([](Style& style) { style.padding({}); });
}

Container& Container::set_scrollable(bool vertical, bool horizontal) {
    m_scroll_vertical = vertical;
    m_scroll_horizontal = horizontal;
    return *this;
}

void Container::apply_theme_defaults(const Theme& theme) {
    configure_all_styles([&theme](Style& style) { style.scrollbar(theme.scrollbar); });
}

void Container::on_layout() {
    // resolve this container before arranging children against its size and padding.
    resolve_layout();
    arrange_children();
}

void Container::resolve_layout() {
    if (has_size()) {
        return;
    }

    assign_size(layout().size_spec().resolve(layout().measured_size(), layout().available_size()));
}

void Container::draw_children() {
    for (const auto& child : children()) {
        if (child->layout().in_flow()) {
            child->draw();
            continue;
        }

        const ImVec2 margin = child->layout_margin();
        Placement placement = child->layout().placement();
        const ImVec2 origin = placement.origin == Anchor::Custom ? placement.origin_position : alignment_factor(placement.origin);
        placement.offset.x += margin.x * (1.0F - 2.0F * origin.x);
        placement.offset.y += margin.y * (1.0F - 2.0F * origin.y);
        const LayoutSize& size_spec = child->layout().size_spec();
        ImVec2 size = child->layout().intrinsic_size();
        const ImVec2 available = content_size(layout().size());
        if (size_spec.width.mode == LayoutSizeMode::Percent) size.x = size_spec.width.resolve(size.x, available.x);
        if (size_spec.height.mode == LayoutSizeMode::Percent) size.y = size_spec.height.resolve(size.y, available.y);
        arrange_child(*child, size, placement);
        child->draw();
    }
}

bool Container::paint() {
    // begin a child window so imgui supplies clipping, scrolling, and cursor management.
    const ComputedStyle& current_style = computed_style();

    ImGuiChildFlags child_flags = ImGuiChildFlags_AlwaysUseWindowPadding;
    ImGuiWindowFlags window_flags = constants::WIDGET_WINDOW_FLAGS | ImGuiWindowFlags_NoBackground;

    if (m_scroll_vertical || m_scroll_horizontal) {
        window_flags &= ~(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    }

    if (m_scroll_horizontal) {
        window_flags |= ImGuiWindowFlags_HorizontalScrollbar;
    }

    const LayoutSize& size_spec = layout().size_spec();
    if (size_spec.width.mode == LayoutSizeMode::Fit) child_flags |= ImGuiChildFlags_AutoResizeX;
    if (size_spec.height.mode == LayoutSizeMode::Fit) child_flags |= ImGuiChildFlags_AutoResizeY;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, current_style.padding());

    // imgui truncates child window positions to integer pixels. round the origin before opening the
    // child window so an animated position keeps the child and following content on the same pixel
    // instead of jumping when the animation reaches its final value.
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const ImVec2 child_position = {std::round(position.x), std::round(position.y)};
    const Rect child_rect = Rect::from_position_size(child_position, layout().size());
    ImGui::SetCursorScreenPos(child_position);

    const ImGuiID child_id = id().empty() ? ImGui::GetID(this) : ImGui::GetID(id().c_str());
    ImDrawList* parent_draw_list = ImGui::GetWindowDrawList();
    draw_blur(
        *parent_draw_list, child_rect, current_style.blur(), current_style.border_radius(), opacity() * current_style.alpha()
    );
    const ImVec2 parent_clip_min = parent_draw_list->GetClipRectMin();
    const ImVec2 parent_clip_max = parent_draw_list->GetClipRectMax();
    ImGui::BeginChild(child_id, layout().size(), child_flags, window_flags);

    const Rect resolved_child_rect = Rect::from_position_size(ImGui::GetWindowPos(), ImGui::GetWindowSize());
    set_layout_rect(resolved_child_rect);
    set_visual_rect(resolved_child_rect);
    ImDrawList* child_draw_list = ImGui::GetWindowDrawList();
    ImGui::PushClipRect(parent_clip_min, parent_clip_max, false);
    const float paint_opacity = std::clamp(ImGui::GetStyle().Alpha, 0.0F, 1.0F);
    draw_box_shadow(
        *child_draw_list, resolved_child_rect, current_style.box_shadow(), current_style.border_radius(), paint_opacity
    );
    ImGui::PopClipRect();
    draw_frame_surface(*child_draw_list, resolved_child_rect, current_style);

    ImGui::PopStyleVar();
    return true;
}

void Container::on_draw_end() {
    // capture fit-size changes before closing the child so input and deferred decorations use its final bounds.
    const ImVec2 window_position = ImGui::GetWindowPos();
    const ImVec2 window_size = ImGui::GetWindowSize();

    const Rect child_rect = Rect::from_position_size(window_position, window_size);
    set_layout_rect(child_rect);
    set_visual_rect(child_rect);

    const ComputedStyle& current_style = computed_style();
    ImColor border = current_style.border_color().value;
    border.Value.w *= std::clamp(ImGui::GetStyle().Alpha, 0.0F, 1.0F);
    ImDrawList* child_draw_list = ImGui::GetWindowDrawList();
    draw_border(*child_draw_list, child_rect, current_style, border);
    ImGui::EndChild();
}
