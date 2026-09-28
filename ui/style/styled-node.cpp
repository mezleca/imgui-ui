#include "styled-node.hpp"

#include "paint-slot.hpp"
#include "../imgui/draw.hpp"

#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

using namespace ui;

void StyledNode::style_changed(void* owner, bool font_changed) {
    auto* node = static_cast<StyledNode*>(owner);
    // font changes invalidate inherited metrics throughout the subtree. other style changes remeasure this node.
    if (font_changed) {
        invalidate_font_cache_subtree(*node);
    } else {
        node->invalidate_measure();
    }
}

StyledNode::StyledNode(std::string id, std::string_view type_name) : Node(std::move(id)), m_type_name(type_name) {
    m_state.set_change_callback(this, &StyledNode::style_changed);
}

StyledNode::~StyledNode() = default;

void StyledNode::set_surface(UI* surface) {
    m_font_cache_valid = false;
    Node::set_surface(surface);
}

StyledNode& StyledNode::set_font(ImFont* font) {
    configure_all_styles([font](Style& style) { style.font(font); });
    return *this;
}

ImFont* StyledNode::font() const {
    const ComputedStyle& current_style = computed_style();
    if (current_style.font() != nullptr) return current_style.font();
    if (m_font_cache_valid) return m_cached_font;

    // resolve inherited fonts from the nearest styled ancestor and cache that result until a style changes.
    for (const Node* ancestor = parent(); ancestor != nullptr; ancestor = ancestor->parent()) {
        const auto* styled_ancestor = dynamic_cast<const StyledNode*>(ancestor);
        if (styled_ancestor == nullptr) continue;

        const ComputedStyle& ancestor_style = styled_ancestor->computed_style();
        if (ancestor_style.font() != nullptr) {
            m_cached_font = ancestor_style.font();
            m_font_cache_valid = true;
            return m_cached_font;
        }
    }

    return ImGui::GetFont();
}

BoxInsets StyledNode::box_insets() const {
    const ComputedStyle& style = computed_style();
    const ImVec2 padding = style.padding();
    const float thickness = style.border_thickness();
    const uint8_t border = style.border();
    return {
        padding.x + ((border & BORDER_LEFT) != 0 ? thickness : 0.0F),
        padding.y + ((border & BORDER_TOP) != 0 ? thickness : 0.0F),
        padding.x + ((border & BORDER_RIGHT) != 0 ? thickness : 0.0F),
        padding.y + ((border & BORDER_BOTTOM) != 0 ? thickness : 0.0F),
    };
}

void StyledNode::set_measured_content_size(ImVec2 size, bool measured_width, bool measured_height) {
    ImGui::PushFont(font());
    const float line_height = ImGui::GetTextLineHeight();
    ImGui::PopFont();

    size.y = std::max(size.y, line_height * computed_style().line_height());
    set_measured_size(outer_size(size), measured_width, measured_height);
}

void StyledNode::invalidate_font_cache_subtree(Node& node) {
    auto* styled = dynamic_cast<StyledNode*>(&node);
    if (styled != nullptr) styled->m_font_cache_valid = false;

    node.invalidate_measure();
    for (const auto& child : node.children()) {
        if (child->removal_pending()) continue;

        invalidate_font_cache_subtree(*child);
    }
}

void StyledNode::draw_surface(ImDrawList& draw_list, Rect rect, const std::optional<Color>& background) const {
    ui::draw_frame(draw_list, rect, computed_style(), effect_registry(), 1.0F, background);
}

PaintSlot& StyledNode::before() {
    if (m_before == nullptr) {
        m_before = std::make_unique<PaintSlot>(this, &StyledNode::style_changed);
    }

    return *m_before;
}

PaintSlot& StyledNode::after() {
    if (m_after == nullptr) {
        m_after = std::make_unique<PaintSlot>(this, &StyledNode::style_changed);
    }

    return *m_after;
}

void StyledNode::remove_before() {
    m_before.reset();
}

void StyledNode::remove_after() {
    m_after.reset();
}

void StyledNode::draw() {
    if (!m_state.is_visible()) {
        return;
    }

    update_cursor();
    const ComputedStyle& current_style = computed_style();
    const PushState push_state = current_style.push(opacity(), font());

    const ImVec2 scale = current_style.scale();
    const float rotation_deg = current_style.rotation();

    if (rotation_deg == 0.0F && scale.x == 1.0F && scale.y == 1.0F) {
        Node::draw();
        ComputedStyle::pop(push_state);
        return;
    }

    struct DrawListSnapshot {
        ImDrawList* draw_list = nullptr;
        int vertex_count = 0;
        ImGuiWindow* window = nullptr;
    };

    ImGuiContext& context = *ImGui::GetCurrentContext();
    const int initial_window_count = context.Windows.Size;
    ImDrawList* background_draw_list = ImGui::GetBackgroundDrawList();
    ImDrawList* foreground_draw_list = ImGui::GetForegroundDrawList();
    std::vector<DrawListSnapshot> draw_list_snapshots;
    draw_list_snapshots.reserve(context.Windows.Size + 2);

    // active windows already contain this frame's vertices. opening an inactive child resets its buffer, so start at zero.
    for (ImGuiWindow* window : context.Windows) {
        const int first_vertex = window->LastFrameActive == context.FrameCount ? window->DrawList->VtxBuffer.Size : 0;
        draw_list_snapshots.push_back({window->DrawList, first_vertex, window});
    }
    draw_list_snapshots.push_back({background_draw_list, background_draw_list->VtxBuffer.Size});
    draw_list_snapshots.push_back({foreground_draw_list, foreground_draw_list->VtxBuffer.Size});

    Node::draw();

    const Rect rect = layout().visual_rect();
    const ImVec2 center = {(rect.min.x + rect.max.x) * 0.5F, (rect.min.y + rect.max.y) * 0.5F};
    const float rotation = rotation_deg * std::numbers::pi_v<float> / 180.0F;
    const float sine = std::sin(rotation);
    const float cosine = std::cos(rotation);

    // apply the transform after node drawing because children may append vertices to other draw lists.
    const auto transform_draw_list = [&](ImDrawList& draw_list, int start) {
        for (int index = start; index < draw_list.VtxBuffer.Size; ++index) {
            ImDrawVert& vertex = draw_list.VtxBuffer[index];
            const ImVec2 offset = {(vertex.pos.x - center.x) * scale.x, (vertex.pos.y - center.y) * scale.y};
            vertex.pos = {
                center.x + (offset.x * cosine) - (offset.y * sine),
                center.y + (offset.x * sine) + (offset.y * cosine),
            };
        }
    };

    for (const DrawListSnapshot& snapshot : draw_list_snapshots) {
        if (snapshot.window != nullptr && snapshot.window->LastFrameActive != context.FrameCount) continue;
        transform_draw_list(*snapshot.draw_list, snapshot.vertex_count);
    }

    for (int index = initial_window_count; index < context.Windows.Size; ++index) {
        transform_draw_list(*context.Windows[index]->DrawList, 0);
    }

    ComputedStyle::pop(push_state);
}

bool StyledNode::on_draw() {
    return paint();
}

bool StyledNode::paint() {
    return true;
}

void StyledNode::advance_frame_state(float dt) {
    m_state.update(dt);
}

void StyledNode::input_state_changed() {
    const InputState input = subtree_input_state();
    set_interaction_style(input.hovered, input.active, input.focused);
    update_cursor();
}

void StyledNode::update_cursor() {
    if (!input_state().hovered) {
        return;
    }

    const ImGuiMouseCursor cursor = style(style_type()).cursor();
    if (cursor != ImGuiMouseCursor_None) ImGui::SetMouseCursor(cursor);
}

void StyledNode::draw_before() {
    if (m_before != nullptr) {
        const Rect rect = layout().visual_rect();
        m_before->paint(effect_registry(), *ImGui::GetWindowDrawList(), rect, rect.inset(computed_style().padding()));
    }
}

void StyledNode::draw_after() {
    if (m_after != nullptr) {
        const Rect rect = layout().visual_rect();
        m_after->paint(effect_registry(), *ImGui::GetForegroundDrawList(), rect, rect.inset(computed_style().padding()));
    }
}
