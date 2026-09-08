#include "styled-node.hpp"

#include "paint-slot.hpp"

#include <imgui_internal.h>

#include <cmath>
#include <vector>

using namespace ui;

StyledNode::StyledNode(std::string id, std::string_view type_name) : Node(std::move(id)), m_type_name(type_name) {
    m_state.set_change_callback(this, &StyledNode::style_changed);
}

StyledNode::~StyledNode() = default;

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

    if (ImGui::GetCurrentContext() == nullptr) {
        Node::draw();
        return;
    }

    update_cursor();
    const ComputedStyle& current_style = computed_style();
    const PushState push_state = current_style.push(opacity(), font());

    const ImVec2 scale = current_style.scale();
    const float rotation = current_style.rotation();
    if (rotation == 0.0F && scale.x == 1.0F && scale.y == 1.0F) {
        Node::draw();
        ComputedStyle::pop(push_state);
        return;
    }

    struct DrawListSnapshot {
        ImDrawList* draw_list = nullptr;
        int vertex_count = 0;
    };

    ImGuiContext& context = *ImGui::GetCurrentContext();
    const int initial_window_count = context.Windows.Size;
    ImDrawList* background_draw_list = ImGui::GetBackgroundDrawList();
    ImDrawList* foreground_draw_list = ImGui::GetForegroundDrawList();
    std::vector<DrawListSnapshot> draw_list_snapshots;
    draw_list_snapshots.reserve(context.Windows.Size + 2);

    // capture the existing vertices so only this subtree is transformed.
    for (ImGuiWindow* window : context.Windows) {
        draw_list_snapshots.push_back({window->DrawList, window->DrawList->VtxBuffer.Size});
    }
    draw_list_snapshots.push_back({background_draw_list, background_draw_list->VtxBuffer.Size});
    draw_list_snapshots.push_back({foreground_draw_list, foreground_draw_list->VtxBuffer.Size});

    Node::draw();

    const Rect rect = layout().visual_rect();
    const ImVec2 center = {(rect.min.x + rect.max.x) * 0.5F, (rect.min.y + rect.max.y) * 0.5F};
    const float sine = std::sin(rotation);
    const float cosine = std::cos(rotation);

    // transform after Node::draw() because children may append vertices to other draw lists.
    const auto transform_draw_list = [&](ImDrawList& draw_list, int start) {
        for (int index = start; index < draw_list.VtxBuffer.Size; ++index) {
            ImDrawVert& vertex = draw_list.VtxBuffer[index];
            const ImVec2 offset = {(vertex.pos.x - center.x) * scale.x, (vertex.pos.y - center.y) * scale.y};
            vertex.pos = {
                center.x + offset.x * cosine - offset.y * sine,
                center.y + offset.x * sine + offset.y * cosine,
            };
        }
    };

    for (const DrawListSnapshot& snapshot : draw_list_snapshots) {
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
    const InputState& input = input_state();
    set_interaction_style(input.hovered, input.active, input.focused);
    update_cursor();
}

void StyledNode::update_cursor() {
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    if (!input_state().hovered) {
        return;
    }

    const ImGuiMouseCursor cursor = style(style_type()).cursor();
    ImGui::SetMouseCursor(cursor == ImGuiMouseCursor_None ? ImGuiMouseCursor_Arrow : cursor);
}

void StyledNode::draw_before() {
    if (m_before != nullptr) {
        const Rect rect = layout().visual_rect();
        m_before->paint(rect, rect.inset(computed_style().padding()));
    }
}

void StyledNode::draw_after() {
    if (m_after != nullptr) {
        const Rect rect = layout().visual_rect();
        m_after->paint(rect, rect.inset(computed_style().padding()));
    }
}
