#include "child-container.hpp"

#include "../constants.hpp"
#include "../imgui/blur.hpp"
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

ChildContainer& ChildContainer::set_center_content_vertically(bool enabled) {
    m_center_content_vertically = enabled;
    return *this;
}

void ChildContainer::on_layout() {
    const ImVec2 size = requested_size();
    // imgui needs an explicit width when the parent leaves it automatic.
    if (size_was_resolved() || !has_size_request() || size.x > 0.0F) {
        return;
    }

    resolve_size(resolve_layout_size(size, ImGui::GetContentRegionAvail()));
}

bool ChildContainer::paint_content() {
    const Style& current_style = style();
    ImGuiChildFlags child_flags = ImGuiChildFlags_AlwaysUseWindowPadding;
    // this node draws the child background and border after beginchild.
    ImGuiWindowFlags window_flags = constants::WIDGET_WINDOW_FLAGS | ImGuiWindowFlags_NoBackground;

    if (!accepts_imgui_input()) {
        // imgui controls only receive input when no retained node owns this child.
        window_flags |= ImGuiWindowFlags_NoInputs;
    }

    if (m_scrollable) {
        window_flags &= ~(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    }

    if (layout().size().x <= 0.0F) child_flags |= ImGuiChildFlags_AutoResizeX;
    if (layout().size().y <= 0.0F) child_flags |= ImGuiChildFlags_AutoResizeY;

    ImVec2 padding = current_style.padding();
    if (m_center_content_vertically && layout().size().y > 0.0F) {
        padding.y = std::max(0.0F, (layout().size().y - ImGui::GetFontSize()) * 0.5F);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);

    if (current_style.use_background_for_scrollbar()) {
        ImVec4 background = current_style.background_color().value.Value;
        background.w *= opacity() * current_style.alpha();
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, background);
    }

    const ImVec2 position = ImGui::GetCursorScreenPos();
    draw_blur(
        {position, {position.x + layout().size().x, position.y + layout().size().y}}, current_style.blur(),
        current_style.border_radius(), current_style.alpha()
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
