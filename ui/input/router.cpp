#include "router.hpp"
#include "../tree/node.hpp"

#include <algorithm>
#include <utility>

using namespace ui;

static int pointer_button_index(PointerButton button) {
    const int index = static_cast<int>(button) - 1;
    return index >= 0 && index < static_cast<int>(POINTER_BUTTON_COUNT) ? index : -1;
}

static bool is_input_target(const Node* node) {
    return node != nullptr && !node->removal_pending() && node->accepts_input();
}

InputRouter::~InputRouter() {
    for (Node* node : m_attached_nodes) {
        node->detach_input_router(*this);
    }
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
        cancel_capture();
        set_hovered({});
        set_input_flag(m_active_node, nullptr, InputFlag::Active);
        set_focus(nullptr);
    }
}

void InputRouter::set_debug_pointer_blocked(bool blocked) {
    if (m_debug_pointer_blocked == blocked) {
        return;
    }

    m_debug_pointer_blocked = blocked;
    if (blocked) {
        set_hovered({});
    }
}

void InputRouter::clear_input_state(Node& subtree) {
    // remove hit regions and their hover flags before clearing focus, capture and stored presses.
    m_hit_test.erase_subtree(subtree);
    std::erase_if(m_hovered_nodes, [&subtree](Node* node) {
        if (!subtree.contains(node)) return false;

        set_input_flag(*node, InputFlag::Hovered, false);
        return true;
    });
    if (m_hovered_nodes.empty()) set_hovered({});

    if (subtree.contains(m_active_node)) set_input_flag(m_active_node, nullptr, InputFlag::Active);
    if (subtree.contains(m_focused_node)) set_focus(nullptr);
    release_pointer(subtree);
}

void InputRouter::detach(Node& subtree) {
    // clear focus without dispatch before discarding references to nodes being destroyed or detached.
    if (subtree.contains(m_focused_node)) set_input_flag(m_focused_node, nullptr, InputFlag::Focused);
    clear_input_state(subtree);

    std::erase_if(m_attached_nodes, [&subtree](Node* node) { return subtree.contains(node); });
}

void InputRouter::refresh_pointer_state(ImVec2 position) {
    if (!ImGui::IsMousePosValid(&position)) {
        return;
    }

    if (m_debug_inspect_mode || m_debug_pointer_blocked) {
        set_hovered({});
        return;
    }

    set_hovered(m_hit_test.route_at(position, EventType::PointerMove));
}

void InputRouter::set_input_flag(Node& node, InputFlag flag, bool enabled) {
    InputState state = node.input_state();
    if (flag == InputFlag::Hovered) state.hovered = enabled;
    if (flag == InputFlag::Active) state.active = enabled;
    if (flag == InputFlag::Focused) state.focused = enabled;
    node.set_input_state(state);
}

void InputRouter::set_input_flag(Node*& current, Node* next, InputFlag flag) {
    if (current == next) return;

    if (current != nullptr) set_input_flag(*current, flag, false);
    current = next;
    if (current != nullptr) set_input_flag(*current, flag, true);
}

void InputRouter::set_hovered(const HitTestIndex::Route& route) {
    Node* owner = route.targets.empty() && route.blocker ? route.blocker->node : nullptr;
    if (!is_input_target(owner)) owner = nullptr;

    for (Node* node : m_hovered_nodes) {
        const bool hit =
            std::any_of(route.targets.begin(), route.targets.end(), [node](const auto& entry) { return entry.node == node; });
        if (!hit && node != owner) set_input_flag(*node, InputFlag::Hovered, false);
    }

    m_hovered_nodes.clear();
    for (const auto& entry : route.targets)
        m_hovered_nodes.push_back(entry.node);
    if (owner != nullptr) m_hovered_nodes.push_back(owner);
    for (Node* node : m_hovered_nodes)
        set_input_flag(*node, InputFlag::Hovered, true);

    if (m_hovered_nodes.empty() && ImGui::GetCurrentContext() != nullptr) ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
}

void InputRouter::register_target(Node& node, Rect rect, InputCallback callback, EventMask events) {
    node.set_input_router(this);
    m_hit_test.add(&node, HitTestIndex::EntryKind::Target, rect, events, std::move(callback));
}

void InputRouter::register_blocker(Rect rect, InputCallback callback, EventMask events) {
    m_hit_test.add(nullptr, HitTestIndex::EntryKind::Blocker, rect, events, std::move(callback));
}

void InputRouter::register_blocker(Node& owner, Rect rect, InputCallback callback, EventMask events) {
    owner.set_input_router(this);
    m_hit_test.add(&owner, HitTestIndex::EntryKind::Blocker, rect, events, std::move(callback));
}

bool InputRouter::capture_pointer(Node& node) {
    if (!is_input_target(&node)) {
        return false;
    }

    if (m_pointer_capture != &node) cancel_capture();
    if (!is_input_target(&node)) return false;

    m_pointer_capture = &node;
    return true;
}

void InputRouter::release_pointer() {
    m_pointer_capture = nullptr;
}

void InputRouter::cancel_capture() {
    Node* captured = std::exchange(m_pointer_capture, nullptr);
    if (captured == nullptr) return;

    // clear ownership before notifying the former owner, whose handler may release or transfer capture.
    UiEvent event = UiEvent::make(EventType::Cancel);
    captured->dispatch_event(event);
}

void InputRouter::release_pointer(Node& subtree) {
    if (subtree.contains(m_pointer_capture)) {
        cancel_capture();
    }

    for (PressedPointer& pressed : m_pressed) {
        std::erase_if(pressed.targets, [&subtree](Node* node) { return subtree.contains(node); });
    }
}

bool InputRouter::set_focus(Node* node) {
    if (node != nullptr && !is_input_target(node)) return false;
    if (m_focused_node == node) return true;

    // notify the old branch, replace its focus flag, then notify the new branch.
    if (m_focused_node != nullptr) {
        UiEvent event = UiEvent::make(EventType::FocusLost);
        dispatch(*m_focused_node, event);
    }

    set_input_flag(m_focused_node, node, InputFlag::Focused);

    if (node != nullptr) {
        UiEvent event = UiEvent::make(EventType::FocusGained);
        dispatch(*node, event);
    }

    return true;
}

void InputRouter::restore_focus(Node& subtree) {
    if (!subtree.contains(m_focused_node)) return;

    for (Node* ancestor = subtree.parent(); ancestor != nullptr; ancestor = ancestor->parent()) {
        if (ancestor->m_input_mode == InputMode::Blocker && is_input_target(ancestor)) {
            set_focus(ancestor);
            return;
        }
    }

    set_focus(nullptr);
}

bool InputRouter::dispatch(UiEvent& event) {
    clear_inactive_targets();

    if (m_debug_inspect_mode) {
        event.stop_propagation();
        return true;
    }

    if (contains(EventMask::Keyboard, event_mask(event.type))) {
        return dispatch_keyboard(event);
    }

    if (!contains(EventMask::Pointer, event_mask(event.type))) {
        return false;
    }

    if (event.type == EventType::PointerMove && m_pointer_capture != nullptr) {
        return dispatch(*m_pointer_capture, event);
    }

    const int button_index = pointer_button_index(event.button);
    // keep the targets reached by this press and carry its native blocking and click prevention into release.
    if (event.type == EventType::PointerDown && button_index >= 0) {
        PressedPointer& pressed = m_pressed[button_index];
        pressed = {};
        dispatch_pointer(event, &pressed.targets);
        pressed.prevent_click = event.default_prevented;
        pressed.native_input_blocked = event.native_input_blocked;
        return event.handled;
    }

    if (event.type != EventType::PointerUp) {
        return dispatch_pointer(event);
    }

    // take the press before callbacks can hide, detach, or release its targets.
    PressedPointer pressed;
    if (button_index >= 0) pressed = std::exchange(m_pressed[button_index], {});
    event.native_input_blocked |= pressed.native_input_blocked;

    Node* captured = m_pointer_capture;
    if (captured != nullptr) {
        dispatch(*captured, event);
        if (m_pointer_capture == captured) release_pointer();
    }

    std::vector<Node*> released;
    if (captured != nullptr && node_at(event.position) == captured) released.push_back(captured);

    dispatch_pointer(event, &released, nullptr, captured);
    set_input_flag(m_active_node, nullptr, InputFlag::Active);

    if (pressed.prevent_click || event.default_prevented ||
        (event.button != PointerButton::Left && event.button != PointerButton::Right)) {
        return event.handled;
    }

    // intersect pressed and released targets before delivering a click through the same pointer route.
    std::erase_if(pressed.targets, [&released](Node* node) {
        return std::find(released.begin(), released.end(), node) == released.end();
    });

    UiEvent click = UiEvent::make(event.button == PointerButton::Left ? EventType::Click : EventType::ContextClick);
    click.position = event.position;
    click.button = event.button;
    click.native_input_blocked = event.native_input_blocked;
    dispatch_pointer(click, nullptr, &pressed.targets);
    event.native_input_blocked |= click.native_input_blocked;
    return event.handled || click.handled;
}

void InputRouter::dispatch_branch(Node& target, UiEvent& event, std::vector<Node*>& visited, Node* scope) {
    event.target = &target;

    for (Node* current = &target; current != nullptr && !event.propagation_stopped; current = current->parent()) {
        if ((scope != nullptr && !scope->contains(current)) || !is_input_target(current)) break;
        if (std::find(visited.begin(), visited.end(), current) != visited.end()) break;

        visited.push_back(current);
        current->dispatch_event(event);
    }
}

bool InputRouter::dispatch_keyboard(UiEvent& event) {
    // snapshot attachment order before handlers can mutate the tree. focus receives the event first.
    const std::vector<Node*> nodes = m_attached_nodes;
    std::vector<Node*> visited;
    Node* scope = nullptr;

    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
        if ((*it)->m_input_mode == InputMode::Blocker && is_input_target(*it)) {
            scope = *it;
            break;
        }
    }

    // preserve native forwarding when a focused descendant restores modal focus while handling this key.
    const bool native_focus_in_scope = scope != nullptr && m_focused_node != scope && scope->contains(m_focused_node);
    if (m_focused_node != nullptr && (scope == nullptr || scope->contains(m_focused_node))) {
        dispatch_branch(*m_focused_node, event, visited, scope);
    }

    for (auto it = nodes.rbegin(); it != nodes.rend() && !event.propagation_stopped; ++it) {
        if ((*it)->m_input_router != this || !is_input_target(*it) || (scope != nullptr && !scope->contains(*it))) continue;

        dispatch_branch(**it, event, visited, scope);
    }

    if (scope != nullptr) {
        if (!native_focus_in_scope) event.block_native_input();
        event.mark_handled();
    }

    return event.handled;
}

bool InputRouter::dispatch_pointer(
    UiEvent& event, std::vector<Node*>* pressed, const std::vector<Node*>* released, Node* captured
) {
    // snapshot before callbacks can open another layer. newly registered targets wait for the next event.
    const auto route = m_hit_test.route_at(event.position, event.type);
    Node* scope = route.blocker ? route.blocker->node : nullptr;
    std::vector<Node*> visited;
    for (Node* node = captured; node != nullptr; node = node->parent())
        visited.push_back(node);

    Node* front = route.targets.empty() ? scope : route.targets.front().node;
    if (event.type != EventType::Scroll && event.type != EventType::Click && event.type != EventType::ContextClick) {
        set_hovered(route);
    }
    if (event.type == EventType::PointerDown) set_input_flag(m_active_node, front, InputFlag::Active);

    for (const auto& entry : route.targets) {
        if (event.propagation_stopped) break;
        if (entry.node->m_input_router != this || !is_input_target(entry.node) ||
            std::find(visited.begin(), visited.end(), entry.node) != visited.end())
            continue;
        if (released != nullptr && std::find(released->begin(), released->end(), entry.node) == released->end()) continue;

        event.target = entry.node;
        if (entry.callback) (*entry.callback)(event);
        if (entry.node->m_input_router != this || !is_input_target(entry.node)) continue;

        if (pressed != nullptr) pressed->push_back(entry.node);
        dispatch_branch(*entry.node, event, visited, scope);
    }

    if (!route.blocker) return event.handled;

    // descendant controls keep native imgui input. only clicks outside the owner's targets invoke the blocking callback.
    if (route.targets.empty()) {
        event.block_native_input();
        if (!event.propagation_stopped) {
            event.target = scope;
            if (route.blocker->callback) (*route.blocker->callback)(event);
            if (is_input_target(scope)) dispatch_branch(*scope, event, visited, scope);
        }
    }
    event.stop_propagation();
    return event.handled;
}

bool InputRouter::dispatch(Node& target, UiEvent& event) {
    event.target = &target;

    for (Node* current = &target; current != nullptr && !event.propagation_stopped; current = current->parent()) {
        if (current->m_removal_pending) {
            break;
        }

        current->dispatch_event(event);
    }

    return event.handled;
}

Node* InputRouter::node_at(ImVec2 position, EventType type) const {
    const auto route = m_hit_test.route_at(position, type);
    if (!route.targets.empty()) return route.targets.front().node;

    return route.blocker ? route.blocker->node : nullptr;
}

InputRouterStats InputRouter::stats() const {
    return {.entry_count = m_hit_test.size(), .entry_checks = m_hit_test.checks()};
}

void InputRouter::clear_inactive_targets() {
    if (!is_input_target(m_focused_node)) {
        set_input_flag(m_focused_node, nullptr, InputFlag::Focused);
    }

    if (!is_input_target(m_pointer_capture)) {
        cancel_capture();
    }

    std::erase_if(m_hovered_nodes, [](Node* node) {
        if (is_input_target(node)) return false;

        set_input_flag(*node, InputFlag::Hovered, false);
        return true;
    });
    if (m_hovered_nodes.empty()) set_hovered({});
    if (!is_input_target(m_active_node)) set_input_flag(m_active_node, nullptr, InputFlag::Active);

    for (PressedPointer& pressed : m_pressed) {
        std::erase_if(pressed.targets, [](Node* node) { return !is_input_target(node); });
    }
}
