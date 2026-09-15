#include "stack-container.hpp"

#include <algorithm>

using namespace ui;

StackContainer::StackContainer(std::string id, StackDirection direction)
    : Container(std::move(id), "StackContainer"), m_direction(direction) {}

StackContainer& StackContainer::set_direction(StackDirection direction) {
    if (m_direction == direction) {
        return *this;
    }

    m_direction = direction;
    const LayoutSize& size = layout().size_spec();
    if (size.width.mode == LayoutSizeMode::Fit || size.height.mode == LayoutSizeMode::Fit) {
        invalidate_measure();
    }
    return *this;
}

StackContainer& StackContainer::set_content_alignment(Anchor alignment) {
    return set_content_alignment(alignment_factor(alignment));
}

StackContainer& StackContainer::set_content_alignment(ImVec2 alignment) {
    const ImVec2 resolved = {
        std::clamp(alignment.x, 0.0F, 1.0F),
        std::clamp(alignment.y, 0.0F, 1.0F),
    };

    if (m_content_alignment.x == resolved.x && m_content_alignment.y == resolved.y) {
        return *this;
    }

    m_content_alignment = resolved;
    return *this;
}

StackContainer& StackContainer::set_spacing(float spacing) {
    const float resolved_spacing = std::max(0.0F, spacing);
    if (m_spacing == resolved_spacing) {
        return *this;
    }

    m_spacing = resolved_spacing;
    const LayoutSize& size = layout().size_spec();
    if (size.width.mode == LayoutSizeMode::Fit || size.height.mode == LayoutSizeMode::Fit) {
        invalidate_measure();
    }
    return *this;
}

bool StackContainer::paint() {
    ImGui::SetNextWindowContentSize(arranged_content_size());
    return Container::paint();
}

StackDirection StackContainer::stack_direction() const {
    return m_direction;
}

float StackContainer::stack_spacing() const {
    return m_spacing;
}

ImVec2 StackContainer::stack_content_alignment() const {
    return m_content_alignment;
}
