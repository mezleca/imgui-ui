#include "router.hpp"
#include "../tree/node.hpp"

#include <algorithm>
#include <utility>

using namespace ui;

InputRouter::~InputRouter() {
    for (Node* node : m_attached_nodes) {
        node->detach_input_router(*this);
    }
}

static bool is_pointer_event(EventType type) {
    return contains(EventMask::Pointer, event_mask(type));
}

static bool is_keyboard_event(EventType type) {
    return contains(EventMask::Keyboard, event_mask(type));
}

static int pointer_button_index(PointerButton button) {
    const int index = static_cast<int>(button) - 1;
    return index >= 0 && index < static_cast<int>(POINTER_BUTTON_COUNT) ? index : -1;
}

static bool is_input_target(const Node* node) {
    return node != nullptr && !node->removal_pending() && node->visible() && node->accepts_input();
}

void InputRouter::begin_frame() {
    m_hit_test.begin_frame(m_attached_nodes.size() + 8);
    clear_inactive_targets();
}

void InputRouter::set_debug_inspect_mode(bool enabled) {
    if (m_debug_inspect_mode == enabled) {
        return;
    }

    m_debug_inspect_mode = enabled;
    if (enabled) {
        set_input_flag(m_hovered_node, nullptr, InputFlag::Hovered);
        set_input_flag(m_active_node, nullptr, InputFlag::Active);
        clear_focus();
    }
}

void InputRouter::set_debug_pointer_blocked(bool blocked) {
    if (m_debug_pointer_blocked == blocked) {
        return;
    }

    m_debug_pointer_blocked = blocked;
    if (blocked) {
        set_input_flag(m_hovered_node, nullptr, InputFlag::Hovered);
    }
}

void InputRouter::erase_entries(Node& node) {
    m_hit_test.erase(node);
}

void InputRouter::clear_input_flag(Node& subtree, Node*& current, InputFlag flag) {
    if (subtree.contains(current)) {
        set_input_flag(current, nullptr, flag);
    }
}

void InputRouter::clear_subtree_entries(Node& subtree) {
    m_hit_test.erase_subtree(subtree);
    clear_input_flag(subtree, m_hovered_node, InputFlag::Hovered);
    clear_input_flag(subtree, m_active_node, InputFlag::Active);
}

void InputRouter::attach_node(Node& node) {
    m_attached_nodes.push_back(&node);
}

void InputRouter::detach_node(Node& node) {
    const auto it = std::find(m_attached_nodes.begin(), m_attached_nodes.end(), &node);
    if (it != m_attached_nodes.end()) m_attached_nodes.erase(it);
}

void InputRouter::detach(Node& subtree) {
    clear_subtree_entries(subtree);
    clear_input_flag(subtree, m_focused_node, InputFlag::Focused);

    if (subtree.contains(m_pointer_capture)) {
        m_pointer_capture = nullptr;
    }

    for (PressedPointer& pressed : m_pressed) {
        if (subtree.contains(pressed.target)) {
            pressed = {};
        }
    }

    std::erase_if(m_attached_nodes, [&subtree](Node* node) { return subtree.contains(node); });
}

void InputRouter::refresh_pointer_state(ImVec2 position) {
    // blocked pointer motion hides the coordinate from imgui after the router has already handled the event.
    if (!ImGui::IsMousePosValid(&position)) {
        return;
    }

    if (m_debug_inspect_mode || m_debug_pointer_blocked) {
        set_input_flag(m_hovered_node, nullptr, InputFlag::Hovered);
        return;
    }

    const HitTestIndex::Entry* blocker = nullptr;
    const HitTestIndex::Entry* target = m_hit_test.resolve(position, EventType::PointerMove, blocker);
    Node* hovered = nullptr;
    if (blocker != nullptr) {
        hovered = blocker->node;
    } else if (target != nullptr) {
        hovered = target->node;
    }
    set_input_flag(m_hovered_node, hovered, InputFlag::Hovered);
}

void InputRouter::set_input_flag(Node*& current, Node* next, InputFlag flag) {
    if (current == next) return;

    if (current != nullptr) {
        InputState state = current->input_state();
        if (flag == InputFlag::Hovered) state.hovered = false;
        if (flag == InputFlag::Active) state.active = false;
        if (flag == InputFlag::Focused) state.focused = false;
        current->set_input_state(state);
    }

    current = next;
    if (current != nullptr) {
        InputState state = current->input_state();
        if (flag == InputFlag::Hovered) state.hovered = true;
        if (flag == InputFlag::Active) state.active = true;
        if (flag == InputFlag::Focused) state.focused = true;
        current->set_input_state(state);
    }

    if (flag == InputFlag::Hovered && next == nullptr && ImGui::GetCurrentContext() != nullptr) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
    }
}

void InputRouter::register_target(Node& node, Rect rect, InputCallback callback) {
    m_hit_test.add(&node, HitTestIndex::EntryKind::Target, rect, EventMask::Pointer, std::move(callback));
}

void InputRouter::register_node(Node& node, bool blocker, Rect input_rect, Rect visual_rect) {
    m_hit_test.register_node(node, blocker, input_rect, visual_rect);
}

void InputRouter::register_blocker(Rect rect, InputCallback callback, EventMask events) {
    m_hit_test.add(nullptr, HitTestIndex::EntryKind::Blocker, rect, events, std::move(callback));
}

void InputRouter::register_blocker(Node& owner, Rect rect, InputCallback callback, EventMask events) {
    m_hit_test.add(&owner, HitTestIndex::EntryKind::Blocker, rect, events, std::move(callback));
}

bool InputRouter::capture_pointer(Node& node) {
    if (!is_input_target(&node)) {
        return false;
    }

    m_pointer_capture = &node;
    return true;
}

void InputRouter::release_pointer() {
    m_pointer_capture = nullptr;
}

void InputRouter::release_pointer(Node& subtree) {
    if (subtree.contains(m_pointer_capture)) {
        release_pointer();
    }

    for (PressedPointer& pressed : m_pressed) {
        if (subtree.contains(pressed.target)) {
            pressed = {};
        }
    }
}

bool InputRouter::set_focus(Node& node) {
    // dispatch focus loss before the next node becomes focused.
    clear_inactive_targets();

    if (!is_input_target(&node)) return false;
    if (m_focused_node == &node) return true;

    if (m_focused_node != nullptr) {
        UiEvent event = UiEvent::make(EventType::FocusLost);
        dispatch(*m_focused_node, event);
    }

    set_input_flag(m_focused_node, &node, InputFlag::Focused);

    UiEvent event = UiEvent::make(EventType::FocusGained);
    dispatch(node, event);
    return true;
}

void InputRouter::clear_focus() {
    if (m_focused_node == nullptr) {
        return;
    }

    UiEvent event = UiEvent::make(EventType::FocusLost);
    dispatch(*m_focused_node, event);
    set_input_flag(m_focused_node, nullptr, InputFlag::Focused);
}

void InputRouter::clear_focus(Node& subtree) {
    if (subtree.contains(m_focused_node)) clear_focus();
}

void InputRouter::restore_focus(Node& subtree) {
    if (!subtree.contains(m_focused_node)) return;

    for (Node* ancestor = subtree.parent(); ancestor != nullptr; ancestor = ancestor->parent()) {
        if (ancestor->m_input_mode == InputMode::Blocker && is_input_target(ancestor)) {
            set_focus(*ancestor);
            return;
        }
    }

    clear_focus();
}

bool InputRouter::dispatch(UiEvent& event) {
    // discard state that points to hidden or disabled nodes.
    clear_inactive_targets();

    // inspection owns the event stream while enabled.
    if (m_debug_inspect_mode) {
        event.mark_handled();
        return true;
    }

    // keyboard input starts at the focused node.
    if (is_keyboard_event(event.type)) {
        if (m_focused_node == nullptr) {
            return false;
        }

        return dispatch(*m_focused_node, event);
    }

    if (!is_pointer_event(event.type)) {
        return false;
    }

    // captured moves stay with the captured node.
    if (event.type == EventType::PointerMove) {
        if (m_pointer_capture != nullptr) {
            return dispatch(*m_pointer_capture, event);
        }

        bool blocked = false;
        const HitTestIndex::Entry* target = pointer_target(event, blocked);

        if (blocked) return true;
        if (target != nullptr) return dispatch_target(*target, event);

        return false;
    }

    // send the release to the captured node before clearing capture.
    if (event.type == EventType::PointerUp) {
        bool handled = false;
        if (m_pointer_capture != nullptr) {
            Node* captured = m_pointer_capture;
            handled = dispatch(*captured, event);
            if (m_pointer_capture == captured) release_pointer();
        }

        const int button_index = pointer_button_index(event.button);
        if (button_index < 0) {
            return handled;
        }

        // consume the press so one release can synthesize one click.
        const PressedPointer pressed = std::exchange(m_pressed[button_index], {});
        // a native-blocked press must also block its matching release.
        event.native_input_blocked |= pressed.native_input_blocked;
        const bool default_prevented = pressed.prevent_click || event.default_prevented;

        bool blocked = false;
        const HitTestIndex::Entry* released = pointer_target(event, blocked);
        set_input_flag(m_active_node, nullptr, InputFlag::Active);

        if (blocked) return true;
        if (default_prevented || !is_input_target(pressed.target)) {
            return handled || event.handled;
        }

        if (released == nullptr || released->node != pressed.target) {
            return handled || event.handled;
        }

        if (event.button == PointerButton::Middle) {
            return handled;
        }

        UiEvent click = UiEvent::make(event.button == PointerButton::Left ? EventType::Click : EventType::ContextClick);
        click.position = event.position;
        click.button = event.button;
        return dispatch_target(*released, click) || handled;
    }

    // resolve the target and blockers once for this event.
    bool blocked = false;
    const HitTestIndex::Entry* target = pointer_target(event, blocked);

    if (blocked) {
        return true;
    }

    if (event.type == EventType::PointerDown && m_focused_node != nullptr &&
        (target == nullptr || !m_focused_node->contains(target->node))) {
        clear_focus();
    }

    if (target == nullptr) {
        return event.handled;
    }

    // clicks require the same target on press and release.
    if (event.type == EventType::PointerDown) {
        set_input_flag(m_active_node, target->node, InputFlag::Active);

        const int button_index = pointer_button_index(event.button);
        if (button_index >= 0) {
            m_pressed[button_index] = {.target = target->node};
        }

        const bool handled = dispatch_target(*target, event);
        if (button_index >= 0) {
            m_pressed[button_index].prevent_click = event.default_prevented;
            m_pressed[button_index].native_input_blocked = event.native_input_blocked;
        }

        return handled;
    }

    return dispatch_target(*target, event);
}

bool InputRouter::dispatch(Node& target, UiEvent& event) {
    return dispatch_bubble(target, event);
}

bool InputRouter::dispatch_bubble(Node& target, UiEvent& event) {
    for (Node* current = &target; current != nullptr && !event.propagation_stopped; current = current->parent()) {
        if (current->m_removal_pending) {
            break;
        }

        current->dispatch_event(event);
    }

    return event.handled;
}

Node* InputRouter::node_at(ImVec2 position) const {
    const HitTestIndex::Entry* target = m_hit_test.target_at(position);
    return target == nullptr ? nullptr : target->node;
}

Node* InputRouter::inspect_node_at(ImVec2 position, EventType type) const {
    const HitTestIndex::Entry* blocker = nullptr;
    const HitTestIndex::Entry* target = m_hit_test.resolve(position, type, blocker);
    if (target != nullptr) {
        return target->node;
    }

    return blocker == nullptr ? nullptr : blocker->node;
}

InputRouterStats InputRouter::stats() const {
    return {.entry_count = m_hit_test.size(), .entry_checks = m_hit_test.checks()};
}

void InputRouter::clear_inactive_targets() {
    if (!is_input_target(m_focused_node)) {
        set_input_flag(m_focused_node, nullptr, InputFlag::Focused);
    }

    if (!is_input_target(m_pointer_capture)) {
        m_pointer_capture = nullptr;
    }

    if (!is_input_target(m_hovered_node)) set_input_flag(m_hovered_node, nullptr, InputFlag::Hovered);
    if (!is_input_target(m_active_node)) set_input_flag(m_active_node, nullptr, InputFlag::Active);

    for (PressedPointer& pressed : m_pressed) {
        if (!is_input_target(pressed.target)) {
            pressed = {};
        }
    }
}

const HitTestIndex::Entry* InputRouter::pointer_target(UiEvent& event, bool& blocked) {
    const HitTestIndex::Entry* blocker = nullptr;
    const HitTestIndex::Entry* target = m_hit_test.resolve(event.position, event.type, blocker);

    if (blocker != nullptr) {
        Node* owner = blocker->node;

        set_input_flag(m_hovered_node, owner, InputFlag::Hovered);
        if (owner != nullptr) {
            if (event.type == EventType::PointerDown) {
                set_input_flag(m_active_node, owner, InputFlag::Active);
            }

            if (!owner->removal_pending()) {
                const bool callback_invoked = m_hit_test.invoke_callback(*blocker, event);
                if (!callback_invoked && !owner->removal_pending()) {
                    dispatch_bubble(*owner, event);
                }
            }
        } else {
            m_hit_test.invoke_callback(*blocker, event);
        }

        // prevent imgui from consuming the same event behind the blocker.
        event.block_native_input();
        event.mark_handled();

        blocked = true;
        return nullptr;
    }

    set_input_flag(m_hovered_node, target == nullptr ? nullptr : target->node, InputFlag::Hovered);
    blocked = false;
    return target;
}

bool InputRouter::dispatch_target(const HitTestIndex::Entry& target, UiEvent& event) {
    Node* node = target.node;
    if (!node->removal_pending()) {
        m_hit_test.invoke_callback(target, event);
        if (!node->removal_pending()) {
            dispatch_bubble(*node, event);
        }
    }
    return event.handled;
}
