#include "style.hpp"

#include <imgui.h>

namespace ui {
    bool Style::push(float opacity, ImFont* effective_font) const {
        const bool push_font = effective_font != nullptr && effective_font != ImGui::GetFont();
        if (push_font) ImGui::PushFont(effective_font);

        const float border_size = (m_border & BORDER_ALL) != 0 ? m_border_thickness : 0.0F;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, m_padding);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, m_border_radius);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, border_size);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, m_border_radius);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, border_size);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * m_alpha * opacity);

        const ImU32 text = m_color.get_col();
        const ImU32 background = m_background_color.get_col();
        ImGui::PushStyleColor(ImGuiCol_Text, text);
        ImGui::PushStyleColor(ImGuiCol_Border, m_border_color.get_col());
        ImGui::PushStyleColor(ImGuiCol_FrameBg, background);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, background);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, background);
        ImGui::PushStyleColor(ImGuiCol_Button, background);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, background);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, background);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, background);
        return push_font;
    }

    void Style::pop(bool font_pushed) {
        ImGui::PopStyleColor(9);
        ImGui::PopStyleVar(6);
        if (font_pushed) ImGui::PopFont();
    }
} // namespace ui
