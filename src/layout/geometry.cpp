#include <imgui-ui/layout/geometry.hpp>

#include <algorithm>

using namespace ui;

float LayoutAxis::intrinsic(float measured) const {
    if (mode == LayoutSizeMode::Fixed) {
        return value;
    }

    return mode == LayoutSizeMode::Fit ? std::max(0.0F, measured) : 0.0F;
}

float LayoutAxis::resolve(float measured, float available) const {
    if (mode == LayoutSizeMode::Grow) {
        return std::max(0.0F, available);
    }

    if (mode == LayoutSizeMode::Percent) {
        return std::max(0.0F, available) * value / 100.0F;
    }

    return intrinsic(measured);
}

bool Placement::operator==(const Placement& other) const {
    return anchor == other.anchor && origin == other.origin && offset.x == other.offset.x && offset.y == other.offset.y &&
           anchor_position.x == other.anchor_position.x && anchor_position.y == other.anchor_position.y &&
           origin_position.x == other.origin_position.x && origin_position.y == other.origin_position.y;
}

ImVec2 ui::alignment_factor(Anchor alignment) {
    switch (alignment) {
        case Anchor::TopLeft:
            return {0.0F, 0.0F};
        case Anchor::TopCenter:
            return {0.5F, 0.0F};
        case Anchor::TopRight:
            return {1.0F, 0.0F};
        case Anchor::CenterLeft:
            return {0.0F, 0.5F};
        case Anchor::Center:
            return {0.5F, 0.5F};
        case Anchor::CenterRight:
            return {1.0F, 0.5F};
        case Anchor::BottomLeft:
            return {0.0F, 1.0F};
        case Anchor::BottomCenter:
            return {0.5F, 1.0F};
        case Anchor::BottomRight:
            return {1.0F, 1.0F};
        case Anchor::Custom:
            return {};
    }
    return {};
}

ImVec2 ui::clamp_position(Rect bounds, ImVec2 size, ImVec2 position) {
    return {
        std::clamp(position.x, bounds.min.x, std::max(bounds.min.x, bounds.max.x - size.x)),
        std::clamp(position.y, bounds.min.y, std::max(bounds.min.y, bounds.max.y - size.y)),
    };
}

ImVec2 NodeLayout::intrinsic_size() const {
    return {
        intrinsic_axis(m_config.size.width, m_measured_size.x, m_box_insets.horizontal(), m_box_sizing),
        intrinsic_axis(m_config.size.height, m_measured_size.y, m_box_insets.vertical(), m_box_sizing),
    };
}

ImVec2 NodeLayout::preferred_size() const {
    return {
        preferred_axis(m_config.size.width, m_measured_size.x, m_box_insets.horizontal(), m_box_sizing),
        preferred_axis(m_config.size.height, m_measured_size.y, m_box_insets.vertical(), m_box_sizing),
    };
}

ImVec2 NodeLayout::resolve_size(ImVec2 available_size) const {
    return {
        resolved_axis(m_config.size.width, m_measured_size.x, available_size.x, m_box_insets.horizontal(), m_box_sizing),
        resolved_axis(m_config.size.height, m_measured_size.y, available_size.y, m_box_insets.vertical(), m_box_sizing),
    };
}

void NodeLayout::set_size(LayoutSize size) {
    m_config.size = size;
    m_has_explicit_size_request = true;
    invalidate_resolved_size();
}

void NodeLayout::set_config(LayoutConfig config) {
    const bool size_changed = m_config.size != config.size;
    m_config = config;
    m_has_explicit_size_request = m_has_explicit_size_request || size_changed;
    m_has_arranged_position = false;

    // placement changes discard arrangement without discarding an unchanged size allocation.
    if (size_changed) {
        invalidate_resolved_size();
    }
}

void NodeLayout::set_measured_size(ImVec2 size, bool measured_width, bool measured_height) {
    m_measured_size = size;

    // measured axes become fit only until an explicit size request fixes the sizing rules.
    if (!m_has_explicit_size_request) {
        if (measured_width) m_config.size.width = fit();
        if (measured_height) m_config.size.height = fit();
    }

    invalidate_resolved_size();
}

void NodeLayout::set_box_insets(BoxInsets insets) {
    if (m_box_insets == insets) {
        return;
    }

    m_box_insets = insets;
    invalidate_resolved_size();
}

void NodeLayout::set_box_sizing(BoxSizing sizing) {
    if (m_box_sizing == sizing) {
        return;
    }

    m_box_sizing = sizing;
    invalidate_resolved_size();
}

void NodeLayout::assign_size(ImVec2 size, bool assigned_by_parent) {
    m_size = size;
    m_has_size = true;
    m_size_assigned_by_parent = assigned_by_parent;
}

void NodeLayout::clear_size_assignment() {
    m_has_size = false;
    m_size_assigned_by_parent = false;
}

void NodeLayout::invalidate_resolved_size() {
    // expose the current intrinsic size until the next layout pass resolves or assigns an allocation.
    m_size = intrinsic_size();
    m_has_size = false;
    m_size_assigned_by_parent = false;
}

void NodeLayout::set_arranged_rects(Rect local_rect, Rect layout_rect) {
    m_local_rect = local_rect;
    m_layout_rect = layout_rect;
    m_visual_rect = layout_rect;
}

float NodeLayout::intrinsic_axis(LayoutAxis axis, float measured, float insets, BoxSizing box_sizing) {
    if (axis.mode == LayoutSizeMode::Fixed) {
        return box_sizing == BoxSizing::ContentBox ? axis.value + insets : std::max(axis.value, insets);
    }

    if (axis.mode == LayoutSizeMode::Grow) return std::max(measured, insets);

    return axis.intrinsic(measured);
}

float NodeLayout::resolved_axis(LayoutAxis axis, float measured, float available, float insets, BoxSizing box_sizing) {
    const float resolved = axis.resolve(measured, available);
    if (axis.mode == LayoutSizeMode::Fixed || axis.mode == LayoutSizeMode::Percent) {
        return box_sizing == BoxSizing::ContentBox ? resolved + insets : std::max(resolved, insets);
    }

    if (axis.mode == LayoutSizeMode::Grow) return std::max(resolved, insets);

    return resolved;
}

float NodeLayout::preferred_axis(LayoutAxis axis, float measured, float insets, BoxSizing box_sizing) {
    return axis.mode == LayoutSizeMode::Percent ? 0.0F : intrinsic_axis(axis, measured, insets, box_sizing);
}
