#include "tree-container.hpp"

#include <algorithm>
#include <imgui.h>

using namespace ui;

TreeContainer::TreeContainer(std::string label, std::string id)
    : Container(id.empty() ? label : std::move(id), "TreeContainer"), m_label(std::move(label)) {
    set_size({grow(), fit()});
}

void TreeContainer::on_measure() {
    Container::on_measure();

    ImVec2 size = layout().intrinsic_size();
    if (!m_open) {
        size.y = 0.0F;
    }

    if (size.x > 0.0F) {
        size.x += ImGui::GetStyle().IndentSpacing;
    }

    size.x = std::max(size.x, ImGui::CalcTextSize(m_label.c_str()).x);
    size.y += ImGui::GetFrameHeightWithSpacing();
    set_measured_size(size, layout().size_spec().width.mode == LayoutSizeMode::Fit, true);
}

bool TreeContainer::paint() {
    const bool open = ImGui::TreeNodeEx(m_label.c_str());
    if (m_open != open) {
        m_open = open;
        invalidate_measure();
    }

    // the TreeNodeEx call already consumed the header; only the remaining outer rect belongs to the body child.
    if (!open) {
        return false;
    }

    const ImVec2 body_position = ImGui::GetCursorScreenPos();
    m_outer_rect = layout().visual_rect();
    m_body_size = {
        std::max(0.0F, m_outer_rect.max.x - body_position.x),
        std::max(0.0F, m_outer_rect.max.y - body_position.y),
    };
    arrange_children();
    return Container::paint();
}

void TreeContainer::on_draw_end() {
    Container::on_draw_end();
    const Rect body_rect = layout().visual_rect();
    m_outer_rect.max.x = std::max(m_outer_rect.max.x, body_rect.max.x);
    m_outer_rect.max.y = std::max(m_outer_rect.max.y, body_rect.max.y);
    set_layout_rect(m_outer_rect);
    set_visual_rect(m_outer_rect);
    ImGui::TreePop();
}

ImVec2 TreeContainer::child_window_size() const {
    return m_body_size;
}

Rect TreeContainer::shadow_rect(Rect) const {
    return m_outer_rect;
}

ImVec2 TreeContainer::child_layout_size() const {
    return content_size(m_body_size);
}
