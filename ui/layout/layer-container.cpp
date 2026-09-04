#include "layer-container.hpp"
#include "../imgui/draw.hpp"

#include <utility>

using namespace ui;

static constexpr ImGuiWindowFlags LAYER_WINDOW_FLAGS =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

LayerContainer::LayerContainer(std::string id, LayerMode mode) : LayerContainer(std::move(id), mode, "LayerContainer") {}

LayerContainer::LayerContainer(std::string id, LayerMode mode, std::string_view type_name)
    : Container(std::move(id), type_name, mode == LayerMode::Window), m_mode(mode) {}

void LayerContainer::resolve_layout() {
    assign_size(ImGui::GetMainViewport()->WorkSize);
}

bool LayerContainer::paint() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    const Rect viewport_rect = Rect::from_position_size(viewport->WorkPos, viewport->WorkSize);
    if (m_mode == LayerMode::Inline) {
        // inline layers paint in the current window and only replace the layout box.
        set_layout_rect(viewport_rect);
        set_visual_rect(viewport_rect);
        draw_frame(viewport_rect, style());
        return true;
    }

    // window layers create a borderless viewport-sized imgui window.
    const bool accepts_input = this->accepts_input();
    ImGuiWindowFlags window_flags = LAYER_WINDOW_FLAGS;
    if (!accepts_input) {
        window_flags |= ImGuiWindowFlags_NoInputs;
    }

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    if (accepts_input && m_focus_requested) {
        ImGui::SetNextWindowFocus();
    }
    m_focus_requested = false;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style().padding());
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{});
    ImGui::Begin(id().c_str(), nullptr, window_flags);

    const Rect window_rect = Rect::from_position_size(ImGui::GetWindowPos(), ImGui::GetWindowSize());
    set_layout_rect(window_rect);
    set_visual_rect(window_rect);

    draw_frame(window_rect, style());
    return true;
}

void LayerContainer::on_draw_end() {
    if (m_mode == LayerMode::Inline) {
        return;
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}
