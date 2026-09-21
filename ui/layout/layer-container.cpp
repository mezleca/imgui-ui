#include "layer-container.hpp"
#include "../constants.hpp"
#include "../imgui/draw.hpp"

#include <utility>

using namespace ui;

static constexpr ImGuiWindowFlags LAYER_WINDOW_FLAGS = constants::WINDOW_FLAGS;

static bool needs_child_window(const ComputedStyle& style) {
    return style.background_color().value.Value.w > 0.0F || style.blur() > 0 || style.box_shadow().color.Value.w > 0.0F ||
           style.border() != BORDER_NONE || style.padding().x > 0.0F || style.padding().y > 0.0F ||
           style.overflow() != Overflow::Visible;
}

LayerContainer::LayerContainer(std::string id, LayerMode mode) : LayerContainer(std::move(id), mode, "LayerContainer") {}

LayerContainer::LayerContainer(std::string id, LayerMode mode, std::string_view type_name)
    : Container(std::move(id), type_name), m_mode(mode) {
    set_layout({.in_flow = false});
}

ImGuiWindowFlags LayerContainer::child_window_flags() const {
    return Container::child_window_flags() | ImGuiWindowFlags_NoMouseInputs;
}

void LayerContainer::resolve_layout() {
    if (has_size()) {
        return;
    }

    if (m_mode == LayerMode::Inline && parent() != nullptr) {
        const Rect parent_content = layout().parent_content_rect();
        if (parent_content.valid()) {
            assign_size(parent_content.size());
            return;
        }
    }

    assign_size(ImGui::GetMainViewport()->WorkSize);
}

bool LayerContainer::paint() {
    return m_mode == LayerMode::Inline ? paint_inline() : paint_window();
}

bool LayerContainer::paint_inline() {
    const ImVec2 scroll = {ImGui::GetScrollX(), ImGui::GetScrollY()};
    const bool scrolled = scroll.x != 0.0F || scroll.y != 0.0F;
    // keep controls under the same imgui parent across frames.
    m_inline_child_window = m_inline_child_window || scrolled || needs_child_window(computed_style());
    if (m_inline_child_window) {
        if (scrolled) {
            const ImVec2 cursor = ImGui::GetCursorPos();
            ImGui::SetCursorPos({cursor.x + scroll.x, cursor.y + scroll.y});
        }
        return Container::paint();
    }

    Rect inline_rect = Rect::from_position_size(ImGui::GetCursorScreenPos(), layout().size());
    if (parent() == nullptr) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        inline_rect = Rect::from_position_size(viewport->WorkPos, viewport->WorkSize);
    }
    set_layout_rect(inline_rect);
    set_visual_rect(inline_rect);
    draw_surface(*ImGui::GetWindowDrawList(), inline_rect);
    return true;
}

bool LayerContainer::paint_window() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const bool accepts_input = this->accepts_input();
    ImGuiWindowFlags window_flags = LAYER_WINDOW_FLAGS;
    // the first window creation may need to move this layer above its parent.
    if (!m_window_initialized) {
        window_flags &= ~ImGuiWindowFlags_NoBringToFrontOnFocus;
    }
    if (!accepts_input) {
        window_flags |= ImGuiWindowFlags_NoInputs;
    }

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, box_insets().window_padding());
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{});
    ImGui::Begin(id().c_str(), nullptr, window_flags);
    m_window_initialized = true;

    const Rect window_rect = Rect::from_position_size(ImGui::GetWindowPos(), ImGui::GetWindowSize());
    set_layout_rect(window_rect);
    set_visual_rect(window_rect);

    draw_surface(*ImGui::GetWindowDrawList(), window_rect);
    return true;
}

void LayerContainer::on_draw_end() {
    if (m_mode == LayerMode::Inline) {
        if (m_inline_child_window) {
            Container::on_draw_end();
        }
        return;
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}
