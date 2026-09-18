#include "container.hpp"
#include "geometry.hpp"
#include "../constants.hpp"
#include "../imgui/draw.hpp"
#include "../imgui/effects/blur/blur.hpp"
#include "../imgui/effects/shadow/shadow.hpp"
#include <algorithm>
#include <cmath>
#include <imgui_internal.h>
#include <utility>
#include <vector>

using namespace ui;

static std::vector<ImVec4> effect_clip_stack;

static ImVec4 current_clip(const ImDrawList& draw_list) {
    const ImVec2 min = draw_list.GetClipRectMin();
    const ImVec2 max = draw_list.GetClipRectMax();
    return {min.x, min.y, max.x, max.y};
}

static ImVec4 content_clip(const ComputedStyle& style, Rect rect) {
    const float thickness = style.border_thickness();
    if ((style.border() & BORDER_LEFT) != 0) rect.min.x += thickness;
    if ((style.border() & BORDER_TOP) != 0) rect.min.y += thickness;
    if ((style.border() & BORDER_RIGHT) != 0) rect.max.x -= thickness + 1.0F;
    if ((style.border() & BORDER_BOTTOM) != 0) rect.max.y -= thickness + 1.0F;
    return {rect.min.x, rect.min.y, rect.max.x, rect.max.y};
}

static ImVec4 intersect_clip(ImVec4 clip, Rect rect) {
    clip.x = std::max(clip.x, rect.min.x);
    clip.y = std::max(clip.y, rect.min.y);
    clip.z = std::min(clip.z, rect.max.x);
    clip.w = std::min(clip.w, rect.max.y);
    return clip;
}

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
    // resolve this container before arranging children against its size and padding.
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
    if (horizontal) {
        content_size.x += spacing;
    } else {
        content_size.y += spacing;
    }

    set_measured_size(outer_size(content_size), fit_width, fit_height);
}

void Container::arrange_children() {
    arrange_children(child_layout_size());
}

void Container::arrange_children(ImVec2 content_size) {
    const bool horizontal = m_direction == StackDirection::Horizontal;
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
        const ImVec2 margin = child->layout_margin();
        const ImVec2 child_size = resolve_stack_child_size(*child, content_size, 0.0F, horizontal);

        fixed_main += axis_extent(margin, horizontal) * 2.0F;
        if (main_axis.mode != LayoutSizeMode::Grow) {
            fixed_main += axis_extent(child_size, horizontal);
        } else {
            fixed_main += child->layout().box_insets().axis(horizontal);
            flexible_weight += main_axis.value;
        }

        if (aligns_content) {
            flow_cross = std::max(flow_cross, axis_extent(child_size, !horizontal) + (axis_extent(margin, !horizontal) * 2.0F));
        }

        ++flow_count;
    }

    const float item_spacing = m_spacing;
    const float spacing = flow_count > 0 ? item_spacing * static_cast<float>(flow_count - 1) : 0.0F;
    const float flexible_main =
        flexible_weight > 0.0F ? std::max(0.0F, available_main - fixed_main - spacing) / flexible_weight : 0.0F;

    ImVec2 cursor{};
    if (aligns_content) {
        const float flow_main = fixed_main + (flexible_main * flexible_weight) + spacing;
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
        ImVec2 child_offset = {cursor.x + margin.x, cursor.y + margin.y};
        if (aligns_content) {
            const float child_cross = axis_extent(child_size, !horizontal) + (axis_extent(margin, !horizontal) * 2.0F);
            const float cross_offset = std::max(0.0F, flow_cross - child_cross) * (horizontal ? alignment.y : alignment.x);
            if (horizontal) {
                child_offset.y += cross_offset;
            } else {
                child_offset.x += cross_offset;
            }
        }

        arrange_child(*child, child_size, {.offset = child_offset});
        m_content_size.x = std::max(m_content_size.x, child_offset.x + child_size.x + margin.x);
        m_content_size.y = std::max(m_content_size.y, child_offset.y + child_size.y + margin.y);

        if (horizontal) {
            cursor.x += child_size.x + (margin.x * 2.0F) + item_spacing;
        } else {
            cursor.y += child_size.y + (margin.y * 2.0F) + item_spacing;
        }
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

Rect Container::shadow_rect(Rect child_rect) const {
    return child_rect;
}

ImVec2 Container::child_layout_size() const {
    return content_size(child_window_size());
}

void Container::draw_children() {
    const ImVec2 available = child_layout_size();
    for (const auto& child : children()) {
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

bool Container::paint() {
    const ComputedStyle& current_style = computed_style();

    ImGuiChildFlags child_flags = ImGuiChildFlags_AlwaysUseWindowPadding;
    ImGuiWindowFlags window_flags = constants::WIDGET_WINDOW_FLAGS | ImGuiWindowFlags_NoBackground;

    const bool scrollable = (m_scroll_vertical || m_scroll_horizontal) && current_style.overflow() != Overflow::Clip;
    if (scrollable) {
        window_flags &= ~(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    }

    if (scrollable && m_scroll_horizontal) {
        window_flags |= ImGuiWindowFlags_HorizontalScrollbar;
    }

    const LayoutSize& size_spec = layout().size_spec();
    if (size_spec.width.mode == LayoutSizeMode::Fit) child_flags |= ImGuiChildFlags_AutoResizeX;
    if (size_spec.height.mode == LayoutSizeMode::Fit) child_flags |= ImGuiChildFlags_AutoResizeY;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, child_window_padding());
    ImGui::SetNextWindowContentSize(child_window_content_size());

    // preserve the allocated max edge when rounding the child origin.
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const ImVec2 child_position = {std::round(position.x), std::round(position.y)};
    const ImVec2 requested_size = child_window_size();
    const ImVec2 child_size = {
        std::max(0.0F, requested_size.x + position.x - child_position.x),
        std::max(0.0F, requested_size.y + position.y - child_position.y),
    };
    ImGui::SetCursorScreenPos(child_position);

    const ImGuiID child_id = id().empty() ? ImGui::GetID(this) : ImGui::GetID(id().c_str());
    // preserve the incoming clip because BeginChild replaces it before the border and visible overflow path run.
    const ImVec4 parent_clip = current_clip(*ImGui::GetWindowDrawList());
    m_parent_clip = parent_clip;
    const ImVec4 parent_effect_clip = effect_clip_stack.empty() ? parent_clip : effect_clip_stack.back();
    ImGui::BeginChild(child_id, child_size, child_flags, window_flags);

    const Rect resolved_child_rect = Rect::from_position_size(ImGui::GetWindowPos(), ImGui::GetWindowSize());
    set_layout_rect(resolved_child_rect);
    set_visual_rect(resolved_child_rect);
    ImDrawList* child_draw_list = ImGui::GetWindowDrawList();
    // effects are the only draws that need the ancestor effect clip.
    if (EffectRegistry* effects = effect_registry();
        effects != nullptr && (current_style.blur() > 0 || current_style.box_shadow().color.Value.w > 0.0F)) {
        const ImRect blur_rect = ImGui::GetCurrentWindow()->InnerRect;
        ImGui::PushClipRect({parent_effect_clip.x, parent_effect_clip.y}, {parent_effect_clip.z, parent_effect_clip.w}, false);
        const float paint_opacity = std::clamp(ImGui::GetStyle().Alpha, 0.0F, 1.0F);
        draw_blur(
            *effects, *child_draw_list, {blur_rect.Min, blur_rect.Max}, current_style.blur(), current_style.border_radius(),
            paint_opacity
        );
        draw_box_shadow(
            *effects, *child_draw_list, shadow_rect(resolved_child_rect), current_style.box_shadow(),
            current_style.border_radius(), paint_opacity
        );
        ImGui::PopClipRect();
    }

    // visible overflow escapes this container but never an ancestor's effect clip.
    effect_clip_stack.push_back(
        current_style.overflow() == Overflow::Visible ? parent_effect_clip
                                                      : intersect_clip(parent_effect_clip, resolved_child_rect)
    );

    // draw the frame under the incoming clip; descendant clipping is applied separately below.
    ImGui::PushClipRect({parent_clip.x, parent_clip.y}, {parent_clip.z, parent_clip.w}, true);
    draw_frame(*child_draw_list, resolved_child_rect, current_style);
    ImGui::PopClipRect();

    // bordered scrollable children still need a manual content clip because imgui's scroll clip does not protect the border.
    m_content_clip_pushed = !scrollable || current_style.border() != BORDER_NONE;
    if (m_content_clip_pushed) {
        if (current_style.overflow() == Overflow::Visible && current_style.border() == BORDER_NONE) {
            ImGui::PushClipRect({parent_clip.x, parent_clip.y}, {parent_clip.z, parent_clip.w}, false);
        } else {
            const ImVec4 clip = content_clip(current_style, resolved_child_rect);
            ImGui::PushClipRect({clip.x, clip.y}, {clip.z, clip.w}, true);
        }
    }

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
    // descendants must stop before the border; the border itself uses the parent clip below.
    if (m_content_clip_pushed) {
        ImGui::PopClipRect();
        m_content_clip_pushed = false;
    }
    if (current_style.border() != BORDER_NONE) {
        ImGui::PushClipRect({m_parent_clip.x, m_parent_clip.y}, {m_parent_clip.z, m_parent_clip.w}, false);
        draw_border(*ImGui::GetWindowDrawList(), child_rect, current_style, border);
        ImGui::PopClipRect();
    }
    ImGui::EndChild();
    effect_clip_stack.pop_back();
}
