#include <imgui-ui/layout/container.hpp>
#include <imgui-ui/layout/geometry.hpp>
#include <imgui-ui/constants.hpp>

#include <algorithm>
#include <imgui_internal.h>
#include <utility>

using namespace ui;

static bool is_flow_child(const Node& child) {
    return !child.removal_pending() && child.visible() && child.layout().in_flow();
}

static ImVec2 stack_available_size(const Node& child, ImVec2 content_size) {
    const ImVec2 margin = child.layout_margin();
    return {std::max(0.0F, content_size.x - (margin.x * 2.0F)), std::max(0.0F, content_size.y - (margin.y * 2.0F))};
}

Container::Container(std::string id, std::string_view type_name)
    : Container(std::move(id), StackDirection::Vertical, type_name) {}

Container::Container(std::string id, StackDirection direction, std::string_view type_name)
    : Widget(std::move(id), type_name, InputMode::Target), m_direction(direction) {
    configure_all_styles([](Style& style) { style.padding({}).box_sizing(BoxSizing::BorderBox); });
}

Container& Container::set_scrollable(bool vertical, bool horizontal) {
    m_scroll.set_axes(vertical, horizontal);
    return *this;
}

void Container::dispatch_event(UiEvent& event) {
    Widget::dispatch_event(event);

    // let widget callbacks prevent wheel scrolling first. block imgui's wheel path even at the limit so only
    // framework ancestors can scroll.
    if (!removal_pending() && !event.propagation_stopped && event.type == EventType::Scroll && !event.default_prevented &&
        scrollable() && computed_style().overflow() != Overflow::Clip) {
        event.block_native_input();
        if (m_scroll.wheel(event.scroll)) event.stop_propagation();
    }
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

void Container::on_measure() {
    const LayoutSize& size = layout().size_spec();
    const bool fit_width = size.width.mode == LayoutSizeMode::Fit;
    const bool fit_height = size.height.mode == LayoutSizeMode::Fit;
    const int main = m_direction == StackDirection::Horizontal ? 0 : 1;
    const int cross = 1 - main;
    ImVec2 content_size{};
    size_t flow_count = 0;

    // sum preferred extents and margins along the flow axis. the cross axis takes the largest child extent.
    for (const auto& child : children()) {
        if (!is_flow_child(*child)) {
            continue;
        }

        const ImVec2 child_size = child->layout().intrinsic_size();
        const ImVec2 margin = child->layout_margin();
        const ImVec2 outer_size = {child_size.x + (margin.x * 2.0F), child_size.y + (margin.y * 2.0F)};

        content_size[main] += outer_size[main];
        content_size[cross] = std::max(content_size[cross], outer_size[cross]);

        ++flow_count;
    }

    const float spacing = flow_count > 0 ? m_spacing * static_cast<float>(flow_count - 1) : 0.0F;
    content_size[main] += spacing;

    // fit includes padding and borders so arranging children can recover the measured content space.
    set_measured_size(outer_size(content_size), fit_width, fit_height);
}

void Container::arrange_children() {
    arrange_children(child_layout_size());
}

void Container::arrange_children(ImVec2 content_size) {
    const int main = m_direction == StackDirection::Horizontal ? 0 : 1;
    const int cross = 1 - main;
    float flow_size = 0.0F;
    float grow_weight = 0.0F;
    m_layout_items.clear();

    // resolve each child's cross size, main minimum and main maximum once. retain vector capacity between frames
    // so redistribution and positioning reuse these results without allocating a new work list on every draw.
    for (const auto& child : children()) {
        if (!is_flow_child(*child)) continue;
        const LayoutSize& request = child->layout().size_spec();
        const LayoutAxis& axis = main == 0 ? request.width : request.height;
        const ImVec2 margin = child->layout_margin();
        const ImVec2 available = stack_available_size(*child, content_size);

        LayoutItem item{child.get(), child->layout().resolve_size(available), std::numeric_limits<float>::infinity(), 0.0F};

        if (axis.mode == LayoutSizeMode::Grow) {
            const float insets = child->layout().box_insets().axis(main == 0);
            item.size[main] = axis.constrain(0.0F, available[main], insets, child->layout().box_sizing());
            item.maximum = axis.constrain(
                std::numeric_limits<float>::infinity(), available[main], insets, child->layout().box_sizing()
            );

            item.weight = axis.value;
            grow_weight += item.weight;
        }

        flow_size += item.size[main] + (margin[main] * 2.0F);
        m_layout_items.push_back(item);
    }

    const float spacing = m_layout_items.empty() ? 0.0F : m_spacing * static_cast<float>(m_layout_items.size() - 1);
    flow_size += spacing;
    float remaining = std::max(0.0F, content_size[main] - flow_size);

    // start at the minima and distribute the remaining extent by weight. reaching a maximum removes that child
    // from later shares. stop after an uncapped pass, which consumes the remainder apart from float rounding.
    while (remaining > 0.0F && grow_weight > 0.0F) {
        const float previous_weight = grow_weight;
        const float share = remaining / grow_weight;

        for (auto& item : m_layout_items) {
            if (item.weight == 0.0F) {
                continue;
            }

            const float old_extent = item.size[main];
            const float extent = std::min(item.maximum, old_extent + (share * item.weight));

            remaining -= extent - old_extent;
            flow_size += extent - old_extent;
            item.size[main] = extent;

            if (extent == item.maximum) {
                grow_weight -= item.weight;
                item.weight = 0.0F;
            }
        }

        if (grow_weight == previous_weight) {
            break;
        }
    }

    float cursor = (content_size[main] - flow_size) * m_content_alignment[main];
    for (const auto& item : m_layout_items) {
        const ImVec2 margin = item.node->layout_margin();
        ImVec2 child_offset = margin;
        child_offset[main] += cursor;
        child_offset[cross] +=
            (content_size[cross] - item.size[cross] - (margin[cross] * 2.0F)) * m_content_alignment[cross];

        arrange_child(*item.node, item.size, {.offset = child_offset});

        cursor += item.size[main] + (margin[main] * 2.0F) + m_spacing;
    }
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
    if (!has_input_mode() && !has_direct_input_child() && !m_scroll.enabled() &&
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
    arrange_children();
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
        const ImVec2 child_available = stack_available_size(*child, available);
        const ImVec2 size = child->layout().resolve_size(child_available);
        arrange_child(*child, size, placement);
        child->draw();
    }
}
