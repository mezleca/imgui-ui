#include "stack-container.hpp"
#include <algorithm>

using namespace ui;

StackContainer::StackContainer(std::string id, StackDirection direction)
    : ChildContainer(std::move(id), "StackContainer"), m_direction(direction) {}

StackContainer& StackContainer::set_direction(StackDirection direction) {
    if (m_direction == direction) {
        return *this;
    }

    m_direction = direction;
    if (m_fit_width || m_fit_height) {
        invalidate_measure();
    }
    return *this;
}

StackDirection StackContainer::direction() const {
    return m_direction;
}

StackContainer& StackContainer::set_spacing(float spacing) {
    const float resolved_spacing = std::max(0.0F, spacing);
    if (m_spacing == resolved_spacing) {
        return *this;
    }

    m_spacing = resolved_spacing;
    if (m_fit_width || m_fit_height) {
        invalidate_measure();
    }
    return *this;
}

float StackContainer::spacing() const {
    return m_spacing;
}

StackContainer& StackContainer::fit_content(bool enabled) {
    if (m_fit_width == enabled && m_fit_height == enabled) {
        return *this;
    }

    m_fit_width = enabled;
    m_fit_height = enabled;
    invalidate_measure();
    return *this;
}

StackContainer& StackContainer::fit_content_width(bool enabled) {
    if (m_fit_width == enabled) {
        return *this;
    }

    m_fit_width = enabled;
    invalidate_measure();
    return *this;
}

StackContainer& StackContainer::fit_content_height(bool enabled) {
    if (m_fit_height == enabled) {
        return *this;
    }

    m_fit_height = enabled;
    invalidate_measure();
    return *this;
}

bool StackContainer::paint_content() {
    ImGui::SetNextWindowContentSize(m_content_size);
    return ChildContainer::paint_content();
}

void StackContainer::on_measure() {
    if (!m_fit_width && !m_fit_height) {
        return;
    }

    ImVec2 content_size{};
    size_t visible_count = 0;
    for (const auto& child : children()) {
        if (!child->visible() || !child->layout().in_flow()) {
            continue;
        }

        const ImVec2 child_size = requested_size_of(*child);
        if (m_direction == StackDirection::Vertical) {
            content_size.x = std::max(content_size.x, child_size.x);
            content_size.y += child_size.y;
        } else {
            content_size.x += child_size.x;
            content_size.y = std::max(content_size.y, child_size.y);
        }
        ++visible_count;
    }

    const float total_spacing = visible_count > 0 ? m_spacing * static_cast<float>(visible_count - 1) : 0.0F;
    if (m_direction == StackDirection::Vertical) {
        content_size.y += total_spacing;
    } else {
        content_size.x += total_spacing;
    }

    const ImVec2 padding = style().padding();
    m_fit_size = {content_size.x + padding.x * 2.0F, content_size.y + padding.y * 2.0F};
}

ImVec2 StackContainer::requested_size_for_layout() const {
    ImVec2 size = requested_size();
    if (m_fit_width) {
        size.x = m_fit_size.x;
    }
    if (m_fit_height) {
        size.y = m_fit_size.y;
    }
    return size;
}

void StackContainer::on_layout() {
    if (m_fit_width || m_fit_height) {
        const ImVec2 requested = requested_size();
        ImVec2 size = {
            m_fit_width ? m_fit_size.x : requested.x,
            m_fit_height ? m_fit_size.y : requested.y,
        };
        const ImVec2 available = ImGui::GetCurrentContext() == nullptr ? ImVec2{} : ImGui::GetContentRegionAvail();
        resolve_size(resolve_layout_size(size, available));
    } else if (!size_was_resolved()) {
        resolve_size(resolve_layout_size(requested_size(), ImGui::GetContentRegionAvail()));
    }

    arrange_children(layout().size());
}

void StackContainer::arrange_children(ImVec2 container_size) {
    const ImVec2 padding = style().padding();
    const ImVec2 content_size = {
        std::max(0.0F, container_size.x - padding.x * 2.0F),
        std::max(0.0F, container_size.y - padding.y * 2.0F),
    };

    const float available_main = m_direction == StackDirection::Horizontal ? content_size.x : content_size.y;

    float fixed_main = 0.0F;
    size_t visible_count = 0;
    size_t flexible_count = 0;

    for (const auto& child : children()) {
        if (!child->visible() || !child->layout().in_flow()) {
            continue;
        }

        const ImVec2 child_size = requested_size_of(*child);
        const float requested_main = m_direction == StackDirection::Horizontal ? child_size.x : child_size.y;

        if (requested_main > 0.0F) {
            fixed_main += requested_main;
        } else {
            ++flexible_count;
        }

        ++visible_count;
    }

    const float spacing = visible_count > 0 ? m_spacing * static_cast<float>(visible_count - 1) : 0.0F;
    const float flexible_main =
        flexible_count > 0 ? std::max(0.0F, available_main - fixed_main - spacing) / static_cast<float>(flexible_count) : 0.0F;
    ImVec2 cursor{};
    m_content_size = content_size;

    for (const auto& child : children()) {
        if (!child->visible() || !child->layout().in_flow()) {
            continue;
        }

        ImVec2 child_size = requested_size_of(*child);

        if (m_direction == StackDirection::Vertical) {
            if (child_size.x <= 0.0F) child_size.x = content_size.x;
            if (child_size.y <= 0.0F) child_size.y = flexible_main;
        } else {
            if (child_size.x <= 0.0F) child_size.x = flexible_main;
            if (child_size.y <= 0.0F) child_size.y = content_size.y;
        }

        // explicit top-left placement prevents imgui item widths and same-line
        // behavior from becoming a second, implicit layout system.
        arrange_child(*child, child_size, Anchor::TopLeft, Origin::TopLeft, cursor);
        m_content_size.x = std::max(m_content_size.x, cursor.x + child_size.x);
        m_content_size.y = std::max(m_content_size.y, cursor.y + child_size.y);

        if (m_direction == StackDirection::Horizontal) {
            cursor.x += child_size.x + m_spacing;
        } else {
            cursor.y += child_size.y + m_spacing;
        }
    }
}
