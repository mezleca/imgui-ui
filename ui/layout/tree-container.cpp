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

    if (open) {
        arrange_children();
    }

    return open;
}

void TreeContainer::draw_children() {
    for (const auto& child : children()) {
        // treenodeex advances imgui's cursor below its header. children must follow that cursor
        // instead of the container's pre-header arranged origin.
        child->draw_at_cursor();
    }
}

void TreeContainer::on_draw_end() {
    ImGui::TreePop();
}
