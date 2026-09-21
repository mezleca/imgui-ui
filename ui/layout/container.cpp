#include "container.hpp"
#include "geometry.hpp"
#include "../constants.hpp"

#include <algorithm>
#include <imgui_internal.h>
#include <utility>

using namespace ui;

static bool is_flow_child(const Node& child) {
    return !child.removal_pending() && child.visible() && child.layout().in_flow();
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

static ImVec2 resolve_stack_child_size(const Node& child, ImVec2 content_size, float flexible_main, bool horizontal) {
    const LayoutSize& layout_size = child.layout().size_spec();
    const LayoutAxis& main_axis = horizontal ? layout_size.width : layout_size.height;
    const ImVec2 margin = child.layout_margin();

    const float main_available = std::max(0.0F, axis_extent(content_size, horizontal) - (axis_extent(margin, horizontal) * 2.0F));
    const float cross_available =
        std::max(0.0F, axis_extent(content_size, !horizontal) - (axis_extent(margin, !horizontal) * 2.0F));
    const ImVec2 available = horizontal ? ImVec2{main_available, cross_available} : ImVec2{cross_available, main_available};
    ImVec2 size = child.layout().resolve_size(available);

    if (main_axis.mode == LayoutSizeMode::Grow) {
        set_axis_extent(size, horizontal, child.layout().box_insets().axis(horizontal) + (flexible_main * main_axis.value));
    }

    return size;
}

Container::Container(std::string id, std::string_view type_name)
    : Container(std::move(id), StackDirection::Vertical, type_name) {}

Container::Container(std::string id, StackDirection direction, std::string_view type_name)
    : Widget(std::move(id), type_name, InputMode::None), m_direction(direction) {
    configure_all_styles([](Style& style) { style.padding({}).box_sizing(BoxSizing::BorderBox); });
}

Container& Container::set_scrollable(bool vertical, bool horizontal) {
    if (m_scroll_vertical == vertical && m_scroll_horizontal == horizontal) {
        return *this;
    }

    m_scroll_vertical = vertical;
    m_scroll_horizontal = horizontal;
    return *this;
}

Container& Container::set_direction(StackDirection direction) {
    if (m_direction == direction) {
        return *this;
    }

    m_direction = direction;
    invalidate_measure();
    return *this;
}

Container& Container::set_content_alignment(Anchor alignment) {
    return set_content_alignment(alignment_factor(alignment));
}

Container& Container::set_content_alignment(ImVec2 alignment) {
    const ImVec2 resolved = {
        std::clamp(alignment.x, 0.0F, 1.0F),
        std::clamp(alignment.y, 0.0F, 1.0F),
    };

    if (m_content_alignment.x == resolved.x && m_content_alignment.y == resolved.y) {
        return *this;
    }

    m_content_alignment = resolved;
    invalidate_measure();
    return *this;
}

Container& Container::set_spacing(float spacing) {
    const float resolved = std::max(0.0F, spacing);
    if (m_spacing == resolved) {
        return *this;
    }

    m_spacing = resolved;
    invalidate_measure();
    return *this;
}

void Container::apply_theme_defaults(const Theme& theme) {
    configure_all_styles([&theme](Style& style) { style.scrollbar(theme.scrollbar); });
}

void Container::on_layout() {
    resolve_layout();
    arrange_children();
}

void Container::resolve_layout() {
    if (has_size()) {
        return;
    }

    assign_size(layout().resolved_size());
}

void Container::on_measure() {
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

        const ImVec2 child_size = child->layout().preferred_size();
        const ImVec2 margin = child->layout_margin();
        const ImVec2 outer_size = {child_size.x + (margin.x * 2.0F), child_size.y + (margin.y * 2.0F)};

        if (horizontal) {
            content_size.x += outer_size.x;
            content_size.y = std::max(content_size.y, outer_size.y);
        } else {
            content_size.x = std::max(content_size.x, outer_size.x);
            content_size.y += outer_size.y;
        }

        ++flow_count;
    }

    const float spacing = flow_count > 0 ? m_spacing * static_cast<float>(flow_count - 1) : 0.0F;
    set_axis_extent(content_size, horizontal, axis_extent(content_size, horizontal) + spacing);

    ImVec2 measured_size = outer_size(content_size);
    if (computed_style().box_sizing() == BoxSizing::BorderBox) {
        const BoxInsets insets = box_insets();
        const ImVec2 padding = computed_style().padding();
        if (fit_width) measured_size.x -= insets.horizontal() - (padding.x * 2.0F);
        if (fit_height) measured_size.y -= insets.vertical() - (padding.y * 2.0F);
    }

    set_measured_size(measured_size, fit_width, fit_height);
}

void Container::arrange_children() {
    arrange_children(child_layout_size());
}

void Container::arrange_children(ImVec2 content_size) {
    const bool horizontal = m_direction == StackDirection::Horizontal;
    const float available_main = axis_extent(content_size, horizontal);

    float fixed_main_extent = 0.0F;
    size_t flow_count = 0;
    float grow_weight = 0.0F;
    const ImVec2 alignment = m_content_alignment;
    const bool aligns_content = alignment.x > 0.0F || alignment.y > 0.0F;
    float flow_cross_extent = 0.0F;

    for (const auto& child : children()) {
        if (!is_flow_child(*child)) {
            continue;
        }

        const LayoutSize& child_layout_size = child->layout().size_spec();
        const LayoutAxis& main_axis = horizontal ? child_layout_size.width : child_layout_size.height;
        const ImVec2 margin = child->layout_margin();
        const ImVec2 child_size = resolve_stack_child_size(*child, content_size, 0.0F, horizontal);

        fixed_main_extent += axis_extent(margin, horizontal) * 2.0F;
        if (main_axis.mode != LayoutSizeMode::Grow) {
            fixed_main_extent += axis_extent(child_size, horizontal);
        } else {
            fixed_main_extent += child->layout().box_insets().axis(horizontal);
            grow_weight += main_axis.value;
        }

        if (aligns_content) {
            flow_cross_extent =
                std::max(flow_cross_extent, axis_extent(child_size, !horizontal) + (axis_extent(margin, !horizontal) * 2.0F));
        }

        ++flow_count;
    }

    const float spacing = flow_count > 0 ? m_spacing * static_cast<float>(flow_count - 1) : 0.0F;
    const float grow_unit =
        grow_weight > 0.0F ? std::max(0.0F, available_main - fixed_main_extent - spacing) / grow_weight : 0.0F;

    ImVec2 cursor{};
    if (aligns_content) {
        const float flow_main = fixed_main_extent + (grow_unit * grow_weight) + spacing;
        const ImVec2 flow_size = horizontal ? ImVec2{flow_main, flow_cross_extent} : ImVec2{flow_cross_extent, flow_main};
        cursor = {(content_size.x - flow_size.x) * alignment.x, (content_size.y - flow_size.y) * alignment.y};
    }
    m_content_size = content_size;

    for (const auto& child : children()) {
        if (!is_flow_child(*child)) {
            continue;
        }

        const ImVec2 child_size = resolve_stack_child_size(*child, content_size, grow_unit, horizontal);
        const ImVec2 margin = child->layout_margin();
        ImVec2 child_offset = {cursor.x + margin.x, cursor.y + margin.y};
        if (aligns_content) {
            const float child_cross = axis_extent(child_size, !horizontal) + (axis_extent(margin, !horizontal) * 2.0F);
            const float cross_offset = std::max(0.0F, flow_cross_extent - child_cross) * (horizontal ? alignment.y : alignment.x);
            set_axis_extent(child_offset, !horizontal, axis_extent(child_offset, !horizontal) + cross_offset);
        }

        arrange_child(*child, child_size, {.offset = child_offset});
        m_content_size.x = std::max(m_content_size.x, child_offset.x + child_size.x + margin.x);
        m_content_size.y = std::max(m_content_size.y, child_offset.y + child_size.y + margin.y);

        set_axis_extent(
            cursor, horizontal,
            axis_extent(cursor, horizontal) + axis_extent(child_size, horizontal) + (axis_extent(margin, horizontal) * 2.0F) +
                m_spacing
        );
    }
}

ImVec2 Container::child_window_content_size() const {
    return m_content_size;
}

ImVec2 Container::child_window_padding() const {
    return box_insets().window_padding();
}

ImVec2 Container::child_window_size() const {
    return layout().size();
}

ImGuiWindowFlags Container::child_window_flags() const {
    ImGuiWindowFlags flags = constants::WIDGET_WINDOW_FLAGS;
    // keep the parent pass-through until this container owns input or scrolling.
    if (!has_input_mode() && !has_direct_input_child() && !m_scroll_vertical && !m_scroll_horizontal &&
        (ImGui::GetCurrentWindow()->Flags & ImGuiWindowFlags_NoMouseInputs) != 0) {
        flags |= ImGuiWindowFlags_NoMouseInputs;
    }
    return flags;
}

Rect Container::shadow_rect(Rect child_rect) const {
    return child_rect;
}

ImVec2 Container::child_layout_size() const {
    return content_size(child_window_size());
}

void Container::draw_children() {
    const ImVec2 available = child_layout_size();
    for (const auto& child : children()) {
        if (child->removal_pending()) {
            continue;
        }

        if (child->layout().in_flow()) {
            child->draw();
            continue;
        }

        const ImVec2 margin = child->layout_margin();
        Placement placement = child->layout().placement();
        const ImVec2 origin = placement.origin == Anchor::Custom ? placement.origin_position : alignment_factor(placement.origin);
        placement.offset.x += margin.x * (1.0F - (2.0F * origin.x));
        placement.offset.y += margin.y * (1.0F - (2.0F * origin.y));
        const ImVec2 size = child->layout().resolve_size(available);
        arrange_child(*child, size, placement);
        child->draw();
    }
}
