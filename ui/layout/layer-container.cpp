#include "layer-container.hpp"

#include "../imgui/draw.hpp"

namespace ui {
    static constexpr ImGuiWindowFlags LAYER_WINDOW_FLAGS =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

    LayerContainer::LayerContainer(std::string id, std::string_view type_name) : Widget(std::move(id), type_name) {}

    void LayerContainer::on_layout() {
        resolve_size(ImGui::GetMainViewport()->WorkSize);
    }

    bool LayerContainer::on_draw() {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowFocus();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style().padding());
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{});
        ImGui::Begin(id().c_str(), nullptr, LAYER_WINDOW_FLAGS);

        m_rect = Rect::from_position_size(ImGui::GetWindowPos(), ImGui::GetWindowSize());
        set_screen_rect(m_rect);
        draw_frame(m_rect, style());
        return true;
    }

    void LayerContainer::on_draw_end() {
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }
} // namespace ui
