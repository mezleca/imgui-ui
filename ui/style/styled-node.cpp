#include "styled-node.hpp"

#include "paint-slot.hpp"

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
    const Style::PushState push_state = style().push(opacity(), font());

    Node::draw();
    Style::pop(push_state);
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

    if (input_state().hovered) {
        const ImGuiMouseCursor cursor = style(style_type()).cursor();
        ImGui::SetMouseCursor(cursor == ImGuiMouseCursor_None ? ImGuiMouseCursor_Arrow : cursor);
        m_cursor_applied = true;
        return;
    }

    if (m_cursor_applied) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
        m_cursor_applied = false;
    }
}

void StyledNode::draw_before() {
    if (m_before != nullptr) {
        const Rect rect = layout().visual_rect();
        m_before->paint(rect, rect.inset(style().padding()));
    }
}

void StyledNode::draw_after() {
    if (m_after != nullptr) {
        const Rect rect = layout().visual_rect();
        m_after->paint(rect, rect.inset(style().padding()));
    }
}
