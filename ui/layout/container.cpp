#include "container.hpp"

#include "../constants.hpp"
#include "../imgui/draw.hpp"
#include "../imgui/effects/blur/blur.hpp"
#include "../imgui/effects/shadow/shadow.hpp"
#include "../style/style.hpp"
#include "geometry.hpp"

#include <utility>

using namespace ui;

Container::Container(std::string id, std::string_view type_name, bool input_target)
    : Widget(std::move(id), type_name, input_target) {
    configure_all_styles([](Style& style) { style.padding({}); });
}

Container& Container::set_scrollable(bool scrollable) {
    m_scrollable = scrollable;
    return *this;
}

void Container::on_layout() {
    // resolve this box first; child arrangement uses the resulting size and padding.
    resolve_layout();
    arrange_children();
}

void Container::resolve_layout() {
    if (has_size()) {
        return;
    }

    assign_size(layout().size_spec().resolve(layout().measured_size(), layout().available_size()));
}

bool Container::paint() {
    // begin a child window so imgui supplies clipping, scrolling, and cursor management.
    const Style& current_style = style();

    ImGuiChildFlags child_flags = ImGuiChildFlags_AlwaysUseWindowPadding;
    ImGuiWindowFlags window_flags = constants::WIDGET_WINDOW_FLAGS | ImGuiWindowFlags_NoBackground;

    if (!accepts_imgui_input()) {
        window_flags |= ImGuiWindowFlags_NoInputs;
    }

    if (m_scrollable) {
        window_flags &= ~(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    }

    const LayoutSize& size_spec = layout().size_spec();
    if (size_spec.width.mode == LayoutSizeMode::Fit) child_flags |= ImGuiChildFlags_AutoResizeX;
    if (size_spec.height.mode == LayoutSizeMode::Fit) child_flags |= ImGuiChildFlags_AutoResizeY;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, current_style.padding());

    if (current_style.use_background_for_scrollbar()) {
        ImVec4 background = current_style.background_color().value.Value;
        background.w *= opacity() * current_style.alpha();
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, background);
    }

    const ImGuiID child_id = id().empty() ? ImGui::GetID(this) : ImGui::GetID(id().c_str());
    ImDrawList* parent_draw_list = ImGui::GetWindowDrawList();
    ImGui::BeginChild(child_id, layout().size(), child_flags, window_flags);

    const Rect child_rect = Rect::from_position_size(ImGui::GetWindowPos(), ImGui::GetWindowSize());
    const float paint_opacity = opacity() * current_style.alpha();
    draw_box_shadow(*parent_draw_list, child_rect, current_style.box_shadow(), current_style.border_radius(), paint_opacity);
    draw_blur(*parent_draw_list, child_rect, current_style.blur(), current_style.border_radius(), paint_opacity);
    draw_frame_surface(*ImGui::GetWindowDrawList(), child_rect, current_style, opacity());

    if (current_style.use_background_for_scrollbar()) {
        ImGui::PopStyleColor();
    }

    ImGui::PopStyleVar();
    return true;
}

void Container::on_draw_end() {
    // copy the actual child-window rect before closing it, then draw the border in the parent list.
    const ImVec2 window_position = ImGui::GetWindowPos();
    const ImVec2 window_size = ImGui::GetWindowSize();

    const Rect child_rect = Rect::from_position_size(window_position, window_size);
    set_layout_rect(child_rect);
    set_visual_rect(child_rect);

    const Style& current_style = style();
    ImColor border = current_style.border_color().value;
    border.Value.w *= opacity() * current_style.alpha();
    ImGui::EndChild();

    draw_border(*ImGui::GetWindowDrawList(), child_rect, current_style, border);
}
