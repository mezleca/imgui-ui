#include <imgui-ui/layout/layer-container.hpp>
#include <imgui-ui/imgui/window.hpp>
#include <imgui-ui/constants.hpp>
#include <imgui-ui/imgui/painter.hpp>

#include <imgui_internal.h>
#include <utility>

using namespace ui;

LayerContainer::LayerContainer(std::string id, LayerMode mode) : LayerContainer(std::move(id), mode, "LayerContainer") {}

LayerContainer::LayerContainer(std::string id, LayerMode mode, std::string_view type_name)
    : Container(std::move(id), type_name), m_mode(mode) {
    set_layout({.in_flow = false});
    set_input_mode(InputMode::None);
}

ImGuiWindowFlags LayerContainer::child_window_flags() const {
    const ImGuiWindowFlags flags = Container::child_window_flags();

    // imgui performs native scrollbar hit testing separately from our input router. an active blocker must accept
    // native mouse hit testing so imgui selects its window instead of hovering the scrollbar underneath.
    // other layers keep NoMouseInputs so their child window does not intercept native input intended for the parent.
    // native hover is resolved after BeginChild, so test tree eligibility without the container's visual hit test here.
    return input_mode() == InputMode::Blocker && Node::accepts_input() // NOLINT(bugprone-parent-virtual-call)
               ? flags
               : flags | ImGuiWindowFlags_NoMouseInputs;
}

ImGuiWindowFlags LayerContainer::window_flags() const {
    return constants::WINDOW_FLAGS;
}

void LayerContainer::on_layout() {
    if (has_parent_size()) {
        return;
    }

    if (m_mode == LayerMode::Inline && parent() != nullptr) {
        const Rect parent_content = layout().parent_content_rect();
        if (parent_content.valid()) {
            assign_size(layout().resolve_size(parent_content.size()));
            return;
        }
    }

    assign_size(layout().resolve_size(ImGui::GetMainViewport()->WorkSize));
}

bool LayerContainer::paint() {
    return m_mode == LayerMode::Inline ? paint_inline() : paint_window();
}

bool LayerContainer::paint_inline() {
    const ImVec2 scroll = {ImGui::GetScrollX(), ImGui::GetScrollY()};
    if (scroll.x != 0.0F || scroll.y != 0.0F) {
        const ImVec2 cursor = ImGui::GetCursorPos();
        ImGui::SetCursorPos({cursor.x + scroll.x, cursor.y + scroll.y});
    }
    return Container::paint();
}

bool LayerContainer::paint_window() {
    const bool accepts_input = this->accepts_input();
    ImGuiWindowFlags flags = window_flags();

    // allow the first begin call to raise the window. later frames keep its existing stacking order.
    if (!m_window_initialized) {
        flags &= ~ImGuiWindowFlags_NoBringToFrontOnFocus;
    }
    if (!accepts_input) {
        flags |= ImGuiWindowFlags_NoInputs;
    }

    ImVec2 position = layout().layout_rect().min;
    if (parent() == nullptr) {
        const ImVec2 viewport_position = ImGui::GetMainViewport()->WorkPos;
        position = {position.x + viewport_position.x, position.y + viewport_position.y};
    }

    // set movable windows' position and resizable windows' size only on first use.
    ImGui::SetNextWindowPos(position, (flags & ImGuiWindowFlags_NoMove) ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(layout().size(), (flags & ImGuiWindowFlags_NoResize) ? ImGuiCond_Always : ImGuiCond_FirstUseEver);

    // native window resizing bypasses container allocation. give imgui the same resolved limits before Begin
    // so user dragging and saved window sizes cannot escape the node's configured range.
    const ImVec2 content =
        layout().parent_content_rect().valid() ? layout().parent_content_rect().size() : ImGui::GetMainViewport()->WorkSize;
    const BoxInsets margin = layout_margin();
    const ImVec2 available = {std::max(0.0F, content.x - margin.horizontal()), std::max(0.0F, content.y - margin.vertical())};
    ImGui::SetNextWindowSizeConstraints(
        layout().constrain_size({}, available),
        layout().constrain_size({std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()}, available)
    );
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, box_insets().window_padding());
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);

    // imgui applies its style minimum after size constraints. use the node's configured minimum instead.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2{});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{});
    ImGui::Begin(id().c_str(), nullptr, flags);
    apply_window_insets(box_insets());
    m_window_initialized = true;

    const Rect window_rect = Rect::from_position_size(ImGui::GetWindowPos(), ImGui::GetWindowSize());
    assign_size(window_rect.size());
    set_layout_rect(window_rect);

    draw_surface(window_rect);
    return true;
}

void LayerContainer::on_draw_end() {
    if (m_mode == LayerMode::Inline) {
        Container::on_draw_end();
        return;
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);
}
