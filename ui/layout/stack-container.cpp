#include "stack-container.hpp"
#include <algorithm>

using namespace ui;

static bool is_flow_child(const Node& child) {
    return child.visible() && child.layout().in_flow();
}

static float axis_extent(ImVec2 size, bool horizontal) {
    return horizontal ? size.x : size.y;
}

static void set_axis_extent(ImVec2& size, bool horizontal, float value) {
    if (horizontal) {
        size.x = value;
    } else {
        size.y = value;
    }
}

static bool has_fit_axis(const LayoutSize& size) {
    return size.width.mode == LayoutSizeMode::Fit || size.height.mode == LayoutSizeMode::Fit;
}

StackContainer::StackContainer(std::string id, StackDirection direction)
    : Container(std::move(id), "StackContainer"), m_direction(direction) {}

StackContainer& StackContainer::set_direction(StackDirection direction) {
    if (m_direction == direction) {
        return *this;
    }

    m_direction = direction;
    if (has_fit_axis(layout().size_spec())) {
        invalidate_measure();
    }

    return *this;
}

StackDirection StackContainer::direction() const {
    return m_direction;
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
    if (has_fit_axis(layout().size_spec())) {
        invalidate_measure();
    }

    return *this;
}

float StackContainer::spacing() const {
    return m_spacing;
}

bool StackContainer::paint() {
    ImGui::SetNextWindowContentSize(m_content_size);
    return Container::paint();
}

void StackContainer::on_measure() {
    // only flow children contribute to fit sizing.
    const LayoutSize size = layout().size_spec();
    const bool fit_width = size.width.mode == LayoutSizeMode::Fit;
    const bool fit_height = size.height.mode == LayoutSizeMode::Fit;

    if (!fit_width && !fit_height) {
        return;
    }

    const bool horizontal = m_direction == StackDirection::Horizontal;
    ImVec2 content_size{};
    size_t flow_count = 0;

    for (const auto& child : children()) {
        if (!is_flow_child(*child)) {
            continue;
        }

        const ImVec2 child_size = child->layout().intrinsic_size();

        if (horizontal) {
            content_size.x += child_size.x;
            content_size.y = std::max(content_size.y, child_size.y);
        } else {
            content_size.x = std::max(content_size.x, child_size.x);
            content_size.y += child_size.y;
        }

        ++flow_count;
    }

    const float total_spacing = flow_count > 0 ? m_spacing * static_cast<float>(flow_count - 1) : 0.0F;

    if (horizontal) {
        content_size.x += total_spacing;
    } else {
        content_size.y += total_spacing;
    }

    const ImVec2 padding = style().padding();
    set_measured_size({content_size.x + padding.x * 2.0F, content_size.y + padding.y * 2.0F}, fit_width, fit_height);
}

void StackContainer::arrange_children() {
    // reserve fixed space, then distribute the remainder by grow weight.
    const bool horizontal = m_direction == StackDirection::Horizontal;
    const ImVec2 container_size = layout().size();
    const ImVec2 padding = style().padding();
    const ImVec2 content_size = {
        std::max(0.0F, container_size.x - padding.x * 2.0F),
        std::max(0.0F, container_size.y - padding.y * 2.0F),
    };

    const float available_main = axis_extent(content_size, horizontal);

    float fixed_main = 0.0F;
    size_t flow_count = 0;
    float flexible_weight = 0.0F;
    const ImVec2 alignment = m_content_alignment;
    const bool aligns_content = alignment.x > 0.0F || alignment.y > 0.0F;
    float flow_cross = 0.0F;

    for (const auto& child : children()) {
        if (!is_flow_child(*child)) {
            continue;
        }

        const LayoutSize& child_layout_size = child->layout().size_spec();
        const LayoutAxis& main_axis = horizontal ? child_layout_size.width : child_layout_size.height;
        const LayoutAxis& cross_axis = horizontal ? child_layout_size.height : child_layout_size.width;
        const ImVec2 child_size = child->layout().intrinsic_size();

        if (main_axis.mode != LayoutSizeMode::Grow) {
            fixed_main += axis_extent(child_size, horizontal);
        } else {
            flexible_weight += main_axis.value;
        }

        if (aligns_content) {
            const float cross_size = cross_axis.mode == LayoutSizeMode::Grow ? axis_extent(content_size, !horizontal)
                                                                             : axis_extent(child_size, !horizontal);
            flow_cross = std::max(flow_cross, cross_size);
        }

        ++flow_count;
    }

    const float spacing = flow_count > 0 ? m_spacing * static_cast<float>(flow_count - 1) : 0.0F;
    const float flexible_main =
        flexible_weight > 0.0F ? std::max(0.0F, available_main - fixed_main - spacing) / flexible_weight : 0.0F;

    ImVec2 cursor{};
    if (aligns_content) {
        const float flow_main = fixed_main + flexible_main * flexible_weight + spacing;
        const ImVec2 flow_size = horizontal ? ImVec2{flow_main, flow_cross} : ImVec2{flow_cross, flow_main};

        cursor = {
            (content_size.x - flow_size.x) * alignment.x,
            (content_size.y - flow_size.y) * alignment.y,
        };
    }
    m_content_size = content_size;

    for (const auto& child : children()) {
        if (!is_flow_child(*child)) {
            continue;
        }

        const ImVec2 child_size = resolve_child_size(*child, content_size, flexible_main);

        // explicit top-left placement prevents imgui item widths and same-line
        // behavior from becoming a second, implicit layout system.
        arrange_child(*child, child_size, {.offset = cursor});
        m_content_size.x = std::max(m_content_size.x, cursor.x + child_size.x);
        m_content_size.y = std::max(m_content_size.y, cursor.y + child_size.y);

        if (horizontal) {
            cursor.x += child_size.x + m_spacing;
        } else {
            cursor.y += child_size.y + m_spacing;
        }
    }
}

ImVec2 StackContainer::resolve_child_size(const Node& child, ImVec2 content_size, float flexible_main) const {
    // grow fills the main axis by weight and the cross axis by the content box.
    ImVec2 size = child.layout().intrinsic_size();

    const bool horizontal = m_direction == StackDirection::Horizontal;
    const LayoutSize& layout_size = child.layout().size_spec();
    const LayoutAxis& main_axis = horizontal ? layout_size.width : layout_size.height;
    const LayoutAxis& cross_axis = horizontal ? layout_size.height : layout_size.width;

    if (main_axis.mode == LayoutSizeMode::Grow) set_axis_extent(size, horizontal, flexible_main * main_axis.value);
    if (cross_axis.mode == LayoutSizeMode::Grow) set_axis_extent(size, !horizontal, axis_extent(content_size, !horizontal));

    return size;
}
