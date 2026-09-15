#include "container.hpp"
#include "geometry.hpp"
#include "../constants.hpp"
#include "../imgui/draw.hpp"
#include "../imgui/effects/blur/blur.hpp"
#include "../imgui/effects/shadow/shadow.hpp"
#include <algorithm>
#include <cmath>
#include <utility>

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

static ImVec2 resolve_stack_child_size(const Node& child, ImVec2 content_size, float flexible_main, bool horizontal) {
    ImVec2 size = child.layout().intrinsic_size();
    const LayoutSize& layout_size = child.layout().size_spec();
    const LayoutAxis& main_axis = horizontal ? layout_size.width : layout_size.height;
    const LayoutAxis& cross_axis = horizontal ? layout_size.height : layout_size.width;
    const ImVec2 margin = child.layout_margin();

    const float main_available = std::max(0.0F, axis_extent(content_size, horizontal) - axis_extent(margin, horizontal) * 2.0F);
    const float cross_available =
        std::max(0.0F, axis_extent(content_size, !horizontal) - axis_extent(margin, !horizontal) * 2.0F);

    set_axis_extent(
        size, horizontal,
        main_axis.mode == LayoutSizeMode::Grow ? flexible_main * main_axis.value
                                               : main_axis.resolve(axis_extent(size, horizontal), main_available)
    );
    set_axis_extent(size, !horizontal, cross_axis.resolve(axis_extent(size, !horizontal), cross_available));
    return size;
}

Container::Container(std::string id, std::string_view type_name) : Widget(std::move(id), type_name, InputMode::None) {
    configure_all_styles([](Style& style) { style.padding({}); });
}

Container& Container::set_scrollable(bool vertical, bool horizontal) {
    m_scroll_vertical = vertical;
    m_scroll_horizontal = horizontal;
    return *this;
}

void Container::apply_theme_defaults(const Theme& theme) {
    configure_all_styles([&theme](Style& style) { style.scrollbar(theme.scrollbar); });
}

void Container::on_layout() {
    // resolve this container before arranging children against its size and padding.
    resolve_layout();
    arrange_children();
}

void Container::resolve_layout() {
    if (has_size()) {
        return;
    }

    assign_size(layout().size_spec().resolve(layout().measured_size(), layout().available_size()));
}

void Container::on_measure() {
    const LayoutSize size = layout().size_spec();
    const bool fit_width = size.width.mode == LayoutSizeMode::Fit;
    const bool fit_height = size.height.mode == LayoutSizeMode::Fit;
    if (!fit_width && !fit_height) {
        return;
    }

    const bool horizontal = stack_direction() == StackDirection::Horizontal;
    ImVec2 content_size{};
    size_t flow_count = 0;

    for (const auto& child : children()) {
        if (!is_flow_child(*child)) {
            continue;
        }

        const ImVec2 child_size = child->layout().intrinsic_size();
        const ImVec2 margin = child->layout_margin();
        const ImVec2 outer_size = {child_size.x + margin.x * 2.0F, child_size.y + margin.y * 2.0F};

        if (horizontal) {
            content_size.x += outer_size.x;
            content_size.y = std::max(content_size.y, outer_size.y);
        } else {
            content_size.x = std::max(content_size.x, outer_size.x);
            content_size.y += outer_size.y;
        }

        ++flow_count;
    }

    const float spacing = flow_count > 0 ? stack_spacing() * static_cast<float>(flow_count - 1) : 0.0F;
    if (horizontal) {
        content_size.x += spacing;
    } else {
        content_size.y += spacing;
    }

    set_measured_size(outer_size(content_size), fit_width, fit_height);
}

void Container::arrange_children() {
    const bool horizontal = stack_direction() == StackDirection::Horizontal;
    const ImVec2 container_size = layout().size();
    const ImVec2 content_size = this->content_size(container_size);
    const float available_main = axis_extent(content_size, horizontal);

    float fixed_main = 0.0F;
    size_t flow_count = 0;
    float flexible_weight = 0.0F;
    const ImVec2 alignment = stack_content_alignment();
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
        const ImVec2 margin = child->layout_margin();
        const float main_available = std::max(0.0F, available_main - axis_extent(margin, horizontal) * 2.0F);

        fixed_main += axis_extent(margin, horizontal) * 2.0F;
        if (main_axis.mode != LayoutSizeMode::Grow) {
            fixed_main += main_axis.resolve(axis_extent(child_size, horizontal), main_available);
        } else {
            flexible_weight += main_axis.value;
        }

        if (aligns_content) {
            const float cross_available =
                std::max(0.0F, axis_extent(content_size, !horizontal) - axis_extent(margin, !horizontal) * 2.0F);
            const float cross_size = cross_axis.resolve(axis_extent(child_size, !horizontal), cross_available);
            flow_cross = std::max(flow_cross, cross_size + axis_extent(margin, !horizontal) * 2.0F);
        }

        ++flow_count;
    }

    const float spacing = flow_count > 0 ? stack_spacing() * static_cast<float>(flow_count - 1) : 0.0F;
    const float flexible_main =
        flexible_weight > 0.0F ? std::max(0.0F, available_main - fixed_main - spacing) / flexible_weight : 0.0F;

    ImVec2 cursor{};
    if (aligns_content) {
        const float flow_main = fixed_main + flexible_main * flexible_weight + spacing;
        const ImVec2 flow_size = horizontal ? ImVec2{flow_main, flow_cross} : ImVec2{flow_cross, flow_main};
        cursor = {(content_size.x - flow_size.x) * alignment.x, (content_size.y - flow_size.y) * alignment.y};
    }
    m_content_size = content_size;

    for (const auto& child : children()) {
        if (!is_flow_child(*child)) {
            continue;
        }

        const ImVec2 child_size = resolve_stack_child_size(*child, content_size, flexible_main, horizontal);
        const ImVec2 margin = child->layout_margin();
        const ImVec2 child_offset = {cursor.x + margin.x, cursor.y + margin.y};

        arrange_child(*child, child_size, {.offset = child_offset});
        m_content_size.x = std::max(m_content_size.x, child_offset.x + child_size.x + margin.x);
        m_content_size.y = std::max(m_content_size.y, child_offset.y + child_size.y + margin.y);

        if (horizontal) {
            cursor.x += child_size.x + margin.x * 2.0F + spacing;
        } else {
            cursor.y += child_size.y + margin.y * 2.0F + spacing;
        }
    }
}

StackDirection Container::stack_direction() const {
    return StackDirection::Vertical;
}

float Container::stack_spacing() const {
    return 0.0F;
}

ImVec2 Container::stack_content_alignment() const {
    return {};
}

const ImVec2& Container::arranged_content_size() const {
    return m_content_size;
}

void Container::draw_children() {
    for (const auto& child : children()) {
        if (child->layout().in_flow()) {
            child->draw();
            continue;
        }

        const ImVec2 margin = child->layout_margin();
        Placement placement = child->layout().placement();
        const ImVec2 origin = placement.origin == Anchor::Custom ? placement.origin_position : alignment_factor(placement.origin);
        placement.offset.x += margin.x * (1.0F - 2.0F * origin.x);
        placement.offset.y += margin.y * (1.0F - 2.0F * origin.y);
        const LayoutSize& size_spec = child->layout().size_spec();
        ImVec2 size = child->layout().intrinsic_size();
        const ImVec2 available = content_size(layout().size());
        if (size_spec.width.mode == LayoutSizeMode::Percent) size.x = size_spec.width.resolve(size.x, available.x);
        if (size_spec.height.mode == LayoutSizeMode::Percent) size.y = size_spec.height.resolve(size.y, available.y);
        arrange_child(*child, size, placement);
        child->draw();
    }
}

bool Container::paint() {
    // begin a child window so imgui supplies clipping, scrolling, and cursor management.
    const ComputedStyle& current_style = computed_style();

    ImGuiChildFlags child_flags = ImGuiChildFlags_AlwaysUseWindowPadding;
    ImGuiWindowFlags window_flags = constants::WIDGET_WINDOW_FLAGS | ImGuiWindowFlags_NoBackground;

    if (m_scroll_vertical || m_scroll_horizontal) {
        window_flags &= ~(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    }

    if (m_scroll_horizontal) {
        window_flags |= ImGuiWindowFlags_HorizontalScrollbar;
    }

    const LayoutSize& size_spec = layout().size_spec();
    if (size_spec.width.mode == LayoutSizeMode::Fit) child_flags |= ImGuiChildFlags_AutoResizeX;
    if (size_spec.height.mode == LayoutSizeMode::Fit) child_flags |= ImGuiChildFlags_AutoResizeY;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, current_style.padding());

    // imgui truncates child window positions to integer pixels. round the origin before opening the
    // child window so an animated position keeps the child and following content on the same pixel
    // instead of jumping when the animation reaches its final value.
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const ImVec2 child_position = {std::round(position.x), std::round(position.y)};
    ImGui::SetCursorScreenPos(child_position);

    const ImGuiID child_id = id().empty() ? ImGui::GetID(this) : ImGui::GetID(id().c_str());
    ImDrawList* parent_draw_list = ImGui::GetWindowDrawList();
    const ImVec2 parent_clip_min = parent_draw_list->GetClipRectMin();
    const ImVec2 parent_clip_max = parent_draw_list->GetClipRectMax();
    ImGui::BeginChild(child_id, layout().size(), child_flags, window_flags);

    const Rect resolved_child_rect = Rect::from_position_size(ImGui::GetWindowPos(), ImGui::GetWindowSize());
    set_layout_rect(resolved_child_rect);
    set_visual_rect(resolved_child_rect);
    ImDrawList* child_draw_list = ImGui::GetWindowDrawList();
    ImGui::PushClipRect(parent_clip_min, parent_clip_max, false);
    const float paint_opacity = std::clamp(ImGui::GetStyle().Alpha, 0.0F, 1.0F);
    draw_blur(*child_draw_list, resolved_child_rect, current_style.blur(), current_style.border_radius(), paint_opacity);
    draw_box_shadow(
        *child_draw_list, resolved_child_rect, current_style.box_shadow(), current_style.border_radius(), paint_opacity
    );
    ImGui::PopClipRect();
    draw_frame_surface(*child_draw_list, resolved_child_rect, current_style);

    ImGui::PopStyleVar();
    return true;
}

void Container::on_draw_end() {
    // capture fit-size changes before closing the child so input and deferred decorations use its final bounds.
    const ImVec2 window_position = ImGui::GetWindowPos();
    const ImVec2 window_size = ImGui::GetWindowSize();

    const Rect child_rect = Rect::from_position_size(window_position, window_size);
    set_layout_rect(child_rect);
    set_visual_rect(child_rect);

    const ComputedStyle& current_style = computed_style();
    ImColor border = current_style.border_color().value;
    border.Value.w *= std::clamp(ImGui::GetStyle().Alpha, 0.0F, 1.0F);
    ImDrawList* child_draw_list = ImGui::GetWindowDrawList();
    draw_border(*child_draw_list, child_rect, current_style, border);
    ImGui::EndChild();
}
