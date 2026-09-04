#include "style.hpp"

#include <imgui.h>

using namespace ui;
static bool same_color(ImVec4 left, ImVec4 right) {
    return left.x == right.x && left.y == right.y && left.z == right.z && left.w == right.w;
}

Style::PushState Style::push(float opacity, ImFont* effective_font) const {
    ImGuiStyle& current = ImGui::GetStyle();
    PushState state;

    state.font_pushed = effective_font != nullptr && effective_font != ImGui::GetFont();
    if (state.font_pushed) ImGui::PushFont(effective_font);

    const auto push_var = [&state](ImGuiStyleVar variable, float value) {
        ImGui::PushStyleVar(variable, value);
        ++state.variables;
    };

    const auto push_vec2_var = [&state](ImGuiStyleVar variable, ImVec2 value) {
        ImGui::PushStyleVar(variable, value);
        ++state.variables;
    };

    const auto push_color = [&current, &state](ImGuiCol color, ImVec4 value) {
        if (same_color(current.Colors[color], value)) {
            return;
        }

        ImGui::PushStyleColor(color, value);
        ++state.colors;
    };

    if (current.FramePadding.x != m_padding.x || current.FramePadding.y != m_padding.y) {
        push_vec2_var(ImGuiStyleVar_FramePadding, m_padding);
    }
    if (current.FrameRounding != m_border_radius) push_var(ImGuiStyleVar_FrameRounding, m_border_radius);
    if (current.FrameBorderSize != 0.0F) push_var(ImGuiStyleVar_FrameBorderSize, 0.0F);

    const float alpha = current.Alpha * m_alpha * opacity;
    if (current.Alpha != alpha) push_var(ImGuiStyleVar_Alpha, alpha);

    const ImVec4 text = m_color.get();
    const ImVec4 border = m_border_color.get();
    const ImVec4 background = m_background_color.get();

    push_color(ImGuiCol_Text, text);
    push_color(ImGuiCol_Border, border);
    push_color(ImGuiCol_FrameBg, background);
    push_color(ImGuiCol_FrameBgHovered, background);
    push_color(ImGuiCol_FrameBgActive, background);
    push_color(ImGuiCol_Button, background);
    push_color(ImGuiCol_ButtonHovered, background);
    push_color(ImGuiCol_ButtonActive, background);
    return state;
}

void Style::pop(PushState state) {
    ImGui::PopStyleColor(state.colors);
    ImGui::PopStyleVar(state.variables);
    if (state.font_pushed) ImGui::PopFont();
}
