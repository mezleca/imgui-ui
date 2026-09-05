#include "computed-style.hpp"

#include <cmath>
#include <imgui.h>
#include <string>
#include <type_traits>
#include <variant>

namespace ui {
    static bool same_color(ImVec4 left, ImVec4 right) {
        return left.x == right.x && left.y == right.y && left.z == right.z && left.w == right.w;
    }

    ComputedStyle::ComputedStyle() {
        const Theme theme{};
        m_color.set(theme.text_color);
        m_border_color.set(theme.border_color);
        m_background_color.set(theme.transparent);
    }

    ComputedStyle::PushState ComputedStyle::push(float opacity, ImFont* effective_font) const {
        ImGuiStyle& current = ImGui::GetStyle();
        PushState state;

        state.font_pushed = effective_font != nullptr && effective_font != ImGui::GetFont();
        if (state.font_pushed) ImGui::PushFont(effective_font);

        const auto push_var = [&state](ImGuiStyleVar variable, auto value) {
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

        if (current.FramePadding.x != m_padding.value.x || current.FramePadding.y != m_padding.value.y) {
            push_var(ImGuiStyleVar_FramePadding, m_padding.value);
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

    void ComputedStyle::pop(PushState state) {
        ImGui::PopStyleColor(state.colors);
        ImGui::PopStyleVar(state.variables);
        if (state.font_pushed) ImGui::PopFont();
    }

    bool ComputedStyle::is_close_to(const ComputedStyle& target, float epsilon) const {
        if (m_font != target.m_font || !m_padding.is_close(target.m_padding, epsilon) ||
            !m_line_height.is_close(target.m_line_height, epsilon) || std::abs(m_alpha - target.m_alpha) > epsilon ||
            m_cursor != target.m_cursor || m_use_background_for_scrollbar != target.m_use_background_for_scrollbar ||
            m_border != target.m_border || m_blur != target.m_blur ||
            std::abs(m_border_thickness - target.m_border_thickness) > epsilon ||
            std::abs(m_border_radius - target.m_border_radius) > epsilon || m_border_style != target.m_border_style ||
            !m_box_shadow.is_close(target.m_box_shadow, epsilon)) {
            return false;
        }

        if (!m_color.is_close(target.m_color, epsilon) || !m_border_color.is_close(target.m_border_color, epsilon) ||
            !m_background_color.is_close(target.m_background_color, epsilon)) {
            return false;
        }

        return m_vars.for_each([&](const std::string& key, const StyleValue& value) {
            const StyleValue* target_value = target.m_vars.find(key);
            if (target_value == nullptr) {
                return true;
            }

            return std::visit(
                [&](const auto& current_value) {
                    using T = std::decay_t<decltype(current_value)>;
                    const T* typed_target = std::get_if<T>(target_value);
                    return typed_target == nullptr || current_value.is_close(*typed_target, epsilon);
                },
                value
            );
        });
    }
} // namespace ui
