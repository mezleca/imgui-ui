#include "child-container.hpp"

#include "../constants.hpp"
#include "../imgui/effects/blur/blur.hpp"
#include "../imgui/draw.hpp"
#include "../style/style.hpp"

using namespace ui;

ChildContainer::ChildContainer(std::string id, std::string_view type_name) : Widget(std::move(id), type_name, false) {
    configure_all_styles([](Style& style) { style.padding({}); });
}

ChildContainer& ChildContainer::set_scrollable(bool scrollable) {
    m_scrollable = scrollable;
    return *this;
}

ChildContainer& ChildContainer::set_center_content(bool horizontal, bool vertical) {
    m_center_content_horizontally = horizontal;
    m_center_content_vertically = vertical;
    return *this;
}

ImVec2 ChildContainer::content_alignment_factor() const {
    return {m_center_content_horizontally ? 0.5F : 0.0F, m_center_content_vertically ? 0.5F : 0.0F};
}

void ChildContainer::on_layout() {
    const ImVec2 size = requested_size();
    // imgui needs an explicit width when the parent leaves it automatic.
    if (size_was_resolved() || !has_size_request() || size.x > 0.0F) {
        return;
    }

    resolve_size(resolve_layout_size(size, ImGui::GetContentRegionAvail()));
}

bool ChildContainer::paint() {
    const Style& current_style = style();
    ImGuiChildFlags child_flags = ImGuiChildFlags_AlwaysUseWindowPadding;
    // this node draws the child background and border after beginchild.
    ImGuiWindowFlags window_flags = constants::WIDGET_WINDOW_FLAGS | ImGuiWindowFlags_NoBackground;

    // imgui controls only receive input when no retained node owns this child.
    if (!accepts_imgui_input()) {
        window_flags |= ImGuiWindowFlags_NoInputs;
    }

    if (m_scrollable) {
        window_flags &= ~(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    }

    if (layout().size().x <= 0.0F) child_flags |= ImGuiChildFlags_AutoResizeX;
    if (layout().size().y <= 0.0F) child_flags |= ImGuiChildFlags_AutoResizeY;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, current_style.padding());

    if (current_style.use_background_for_scrollbar()) {
        ImVec4 background = current_style.background_color().value.Value;
        background.w *= opacity() * current_style.alpha();
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, background);
    }

    const ImVec2 position = ImGui::GetCursorScreenPos();
    draw_blur(
        *ImGui::GetWindowDrawList(), {position, {position.x + layout().size().x, position.y + layout().size().y}},
        current_style.blur(), current_style.border_radius(), current_style.alpha()
    );

    if (id().empty()) {
        ImGui::BeginChild(ImGui::GetID(this), layout().size(), child_flags, window_flags);
    } else {
        ImGui::BeginChild(id().c_str(), layout().size(), child_flags, window_flags);
    }

    if (current_style.use_background_for_scrollbar()) {
        ImGui::PopStyleColor();
    }

    ImGui::PopStyleVar();
    draw_frame(Rect::from_position_size(ImGui::GetWindowPos(), ImGui::GetWindowSize()), current_style, opacity());
    return true;
}

void ChildContainer::on_draw_end() {
    const ImVec2 window_position = ImGui::GetWindowPos();
    const ImVec2 window_size = ImGui::GetWindowSize();

    m_child_rect = Rect::from_position_size(window_position, window_size);
    set_screen_rect(m_child_rect);

    const Style& current_style = style();
    ImColor border = current_style.border_color().value;
    border.Value.w *= opacity() * current_style.alpha();
    ImGui::EndChild();

    draw_border(*ImGui::GetWindowDrawList(), m_child_rect, current_style, border);
}
