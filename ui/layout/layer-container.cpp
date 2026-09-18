#include "layer-container.hpp"
#include "../constants.hpp"
#include "../imgui/draw.hpp"

#include <utility>

using namespace ui;

static constexpr ImGuiWindowFlags LAYER_WINDOW_FLAGS = constants::WINDOW_FLAGS;

// a child window gives styled padding, backgrounds, borders, and effects a local clip and draw list.
static bool needs_child_scope(const ComputedStyle& style) {
    return style.background_color().value.Value.w > 0.0F || style.blur() > 0 || style.box_shadow().color.Value.w > 0.0F ||
           style.border() != BORDER_NONE || style.padding().x > 0.0F || style.padding().y > 0.0F;
}

LayerContainer::LayerContainer(std::string id, LayerMode mode) : LayerContainer(std::move(id), mode, "LayerContainer") {}

LayerContainer::LayerContainer(std::string id, LayerMode mode, std::string_view type_name)
    : Container(std::move(id), type_name), m_mode(mode) {
    set_layout({.in_flow = false});
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
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    const Rect viewport_rect = Rect::from_position_size(viewport->WorkPos, viewport->WorkSize);
    if (m_mode == LayerMode::Inline) {
        const ImVec2 scroll = {ImGui::GetScrollX(), ImGui::GetScrollY()};
        // a scrolled inline layer must enter the parent content coordinate space before arranging descendants.
        if (scroll.x != 0.0F || scroll.y != 0.0F) {
            const ImVec2 cursor = ImGui::GetCursorPos();
            ImGui::SetCursorPos({cursor.x + scroll.x, cursor.y + scroll.y});
        }

        // padding changes descendant layout, so it needs the same child scope as visible frame effects.
        m_inline_child_scope = m_inline_child_scope || needs_child_scope(computed_style());
        if (m_inline_child_scope) {
            return Container::paint();
        }

        // plain inline layers only replace the layout box in the current window.
        const Rect inline_rect =
            parent() == nullptr ? viewport_rect : Rect::from_position_size(ImGui::GetCursorScreenPos(), layout().size());
        set_layout_rect(inline_rect);
        set_visual_rect(inline_rect);
        draw_surface(*ImGui::GetWindowDrawList(), inline_rect);
        return true;
    }

    // window layers use a borderless viewport-sized imgui window.
    const bool accepts_input = this->accepts_input();
    ImGuiWindowFlags window_flags = LAYER_WINDOW_FLAGS;
    // allow the first window creation to reorder the layer above its parent window.
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
    // close the child opened by container painting while retaining the flag for the next frame.
    if (m_mode == LayerMode::Inline) {
        if (m_inline_child_scope) {
            Container::on_draw_end();
        }
        return;
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}
