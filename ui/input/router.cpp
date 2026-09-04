#include "router.hpp"
#include "../tree/node.hpp"

#include <algorithm>
#include <optional>
#include <utility>

using namespace ui;

InputRouter::~InputRouter() {
    for (Node* node : m_attached_nodes) {
        if (node != nullptr) node->detach_input_router(*this);
    }
}

static bool is_pointer_event(EventType type) {
    return contains(EventMask::Pointer, event_mask(type));
}

static bool is_keyboard_event(EventType type) {
    return contains(EventMask::Keyboard, event_mask(type));
}

static std::optional<std::size_t> pointer_button_index(PointerButton button) {
    switch (button) {
        case PointerButton::Left:
            return 0;
        case PointerButton::Right:
            return 1;
        case PointerButton::Middle:
            return 2;
        case PointerButton::None:
            return std::nullopt;
    }

    return std::nullopt;
}

static std::optional<EventType> click_event_type(PointerButton button) {
    switch (button) {
        case PointerButton::Left:
            return EventType::Click;
        case PointerButton::Right:
            return EventType::ContextClick;
        case PointerButton::Middle:
        case PointerButton::None:
            return std::nullopt;
    }

    return std::nullopt;
}

static bool is_input_target(const Node* node) {
    return node != nullptr && node->visible() && node->accepts_input();
}

void InputRouter::begin_frame() {
    m_entries.clear();
    m_has_blockers = false;
    m_has_observers = false;

    if (m_entries.capacity() < m_attached_nodes.size() + 8) {
        m_entries.reserve(m_attached_nodes.size() + 8);
    }

    m_stats = {};
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
    std::erase_if(m_entries, [&node](const InputEntry& entry) { return entry.node == &node || entry.owner == &node; });
}

void InputRouter::clear_input_flag(Node& subtree, Node*& current, InputFlag flag) {
    if (subtree.contains(current)) {
        set_input_flag(current, nullptr, flag);
    }
}

void InputRouter::clear_subtree_entries(Node& subtree) {
    std::erase_if(m_entries, [&subtree](const InputEntry& entry) {
        return subtree.contains(entry.node) || subtree.contains(entry.owner);
    });
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
    if (m_debug_inspect_mode || m_debug_pointer_blocked) {
        set_input_flag(m_hovered_node, nullptr, InputFlag::Hovered);
        return;
    }

    const InputEntry* target = target_at(position, EventType::PointerMove);
    if (m_has_blockers &&
        blocking_entry_at(position, EventType::PointerMove, target == nullptr ? nullptr : target->node) != nullptr) {
        target = nullptr;
    }
    set_input_flag(m_hovered_node, target == nullptr ? nullptr : target->node, InputFlag::Hovered);
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
}

void InputRouter::target(Node& node, Rect rect, InputCallback callback) {
    add_entry(&node, nullptr, InputKind::Target, rect, EventMask::Pointer, std::move(callback));
}

void InputRouter::add_entry(Node* node, Node* owner, InputKind kind, Rect rect, EventMask events, InputCallback callback) {
    m_has_blockers |= kind == InputKind::Blocker;
    m_has_observers |= kind == InputKind::Observer;
    m_entries.push_back(InputEntry{node, owner, rect, events, std::move(callback), kind});
}

void InputRouter::register_node(Node& node, bool blocker, Rect input_rect, Rect visual_rect) {
    if (!visual_rect.valid()) {
        return;
    }

    if (!input_rect.valid()) {
        input_rect = node.hit_rect(visual_rect);
    } else {
        input_rect.min.x += visual_rect.min.x;
        input_rect.min.y += visual_rect.min.y;
        input_rect.max.x += visual_rect.min.x;
        input_rect.max.y += visual_rect.min.y;
    }

    if (ImGui::GetCurrentContext() != nullptr) {
        const ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const ImVec2 clip_min = draw_list->GetClipRectMin();
        const ImVec2 clip_max = draw_list->GetClipRectMax();
        input_rect.min.x = std::max(input_rect.min.x, clip_min.x);
        input_rect.min.y = std::max(input_rect.min.y, clip_min.y);
        input_rect.max.x = std::min(input_rect.max.x, clip_max.x);
        input_rect.max.y = std::min(input_rect.max.y, clip_max.y);
    }

    if (!input_rect.valid()) {
        return;
    }

    const InputKind kind = blocker ? InputKind::Blocker : InputKind::Target;
    add_entry(blocker ? nullptr : &node, blocker ? &node : nullptr, kind, input_rect, EventMask::Pointer, {});
}

void InputRouter::block(Rect rect, InputCallback callback, EventMask events) {
    add_entry(nullptr, nullptr, InputKind::Blocker, rect, events, std::move(callback));
}

void InputRouter::block(Node& owner, Rect rect, InputCallback callback, EventMask events) {
    add_entry(nullptr, &owner, InputKind::Blocker, rect, events, std::move(callback));
}

void InputRouter::observe(Rect rect, InputCallback callback, EventMask events) {
    add_entry(nullptr, nullptr, InputKind::Observer, rect, events, std::move(callback));
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
    // notify the old target before the new target.
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

    // observers also receive native click events.
    if (event.type == EventType::Click || event.type == EventType::ContextClick) {
        notify_observers(event);
    }

    // captured moves stay with the drag origin.
    if (event.type == EventType::PointerMove) {
        if (m_pointer_capture != nullptr) {
            return dispatch(*m_pointer_capture, event);
        }

        bool blocked = false;
        const InputEntry* target = pointer_target(event, blocked);

        if (blocked) return true;
        if (target != nullptr) return dispatch_target(*target, event);

        return false;
    }

    // release the drag before clearing capture.
    if (event.type == EventType::PointerUp) {
        bool handled = false;
        if (m_pointer_capture != nullptr) {
            Node* captured = m_pointer_capture;
            handled = dispatch(*captured, event);
            if (m_pointer_capture == captured) release_pointer();
        }

        const std::optional<std::size_t> button_index = pointer_button_index(event.button);
        if (!button_index.has_value()) {
            return handled;
        }

        // consume the press so one release can synthesize one click.
        const PressedPointer pressed = std::exchange(m_pressed[*button_index], {});
        const bool default_prevented = pressed.prevent_click || event.default_prevented;

        bool blocked = false;
        const InputEntry* released = pointer_target(event, blocked);
        set_input_flag(m_active_node, nullptr, InputFlag::Active);

        if (blocked) return true;
        if (default_prevented || !is_input_target(pressed.target)) {
            return handled || event.handled;
        }

        if (released == nullptr || released->node != pressed.target) {
            return handled || event.handled;
        }

        const std::optional<EventType> click_type = click_event_type(event.button);
        if (!click_type.has_value()) {
            return handled;
        }

        UiEvent click = UiEvent::make(*click_type);
        click.position = event.position;
        click.button = event.button;
        notify_observers(click);
        return dispatch_target(*released, click) || handled;
    }

    // resolve the target and blockers once for this event.
    bool blocked = false;
    const InputEntry* target = pointer_target(event, blocked);
    if (event.type == EventType::PointerDown && m_focused_node != nullptr &&
        (target == nullptr || !m_focused_node->contains(target->node))) {
        clear_focus();
    }

    if (blocked) {
        return true;
    }

    if (target == nullptr) {
        return event.handled;
    }

    // clicks require the same target on press and release.
    if (event.type == EventType::PointerDown) {
        set_input_flag(m_active_node, target->node, InputFlag::Active);

        const std::optional<std::size_t> button_index = pointer_button_index(event.button);
        if (button_index.has_value()) {
            m_pressed[*button_index] = {.target = target->node};
        }

        const bool handled = dispatch_target(*target, event);
        if (button_index.has_value()) {
            m_pressed[*button_index].prevent_click = event.default_prevented;
        }

        return handled;
    }

    return dispatch_target(*target, event);
}

bool InputRouter::dispatch(Node& target, UiEvent& event) {
    Node* current = &target;

    while (current != nullptr && !event.propagation_stopped) {
        Node* next = current->parent();
        current->dispatch_event(event);

        current = next;
    }

    return event.handled;
}

Node* InputRouter::node_at(ImVec2 position) const {
    const InputEntry* target = target_at(position);
    return target == nullptr ? nullptr : target->node;
}

InputRouterStats InputRouter::stats() const {
    InputRouterStats result = m_stats;
    result.entry_count = m_entries.size();
    return result;
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

const InputRouter::InputEntry* InputRouter::pointer_target(UiEvent& event, bool& blocked) {
    // blockers run before targets and clear hover behind them.
    const InputEntry* target = target_at(event.position, event.type);
    if (m_has_blockers) {
        const InputEntry* blocker = blocking_entry_at(event.position, event.type, target == nullptr ? nullptr : target->node);
        if (blocker != nullptr) {
            set_input_flag(m_hovered_node, nullptr, InputFlag::Hovered);
            if (blocker->callback) {
                blocker->callback(event);
            } else if (blocker->owner != nullptr) {
                dispatch(*blocker->owner, event);
            }
            event.mark_handled();
            blocked = true;
            return nullptr;
        }
    }

    set_input_flag(m_hovered_node, target == nullptr ? nullptr : target->node, InputFlag::Hovered);
    blocked = false;
    return target;
}

const InputRouter::InputEntry* InputRouter::target_at(ImVec2 position, EventType type) const {
    const InputEntry* target = nullptr;
    for (auto it = m_entries.rbegin(); it != m_entries.rend(); ++it) {
        ++m_stats.entry_checks;

        if (it->kind != InputKind::Target || it->node == nullptr || !it->rect.contains(position) ||
            !contains(it->events, event_mask(type))) {
            continue;
        }

        if (!is_input_target(it->node)) {
            continue;
        }

        if (target == nullptr) {
            target = &*it;
            continue;
        }

        // prefer the deeper node when entries overlap.
        if (target->node->contains(it->node)) {
            target = &*it;
        }
    }

    return target;
}

const InputRouter::InputEntry* InputRouter::blocking_entry_at(ImVec2 position, EventType type, const Node* target) const {
    const EventMask mask = event_mask(type);
    if (mask == EventMask::None) {
        return nullptr;
    }

    for (auto it = m_entries.rbegin(); it != m_entries.rend(); ++it) {
        ++m_stats.entry_checks;
        if (it->kind == InputKind::Blocker && it->rect.contains(position) && contains(it->events, mask) &&
            (it->owner == nullptr || (it->owner->visible() && it->owner->enabled() && !it->owner->contains(target)))) {
            return &*it;
        }
    }

    return nullptr;
}

void InputRouter::notify_observers(UiEvent& event) {
    if (!m_has_observers) {
        return;
    }

    const EventMask mask = event_mask(event.type);
    for (auto it = m_entries.rbegin(); it != m_entries.rend(); ++it) {
        ++m_stats.entry_checks;
        if (it->kind != InputKind::Observer || !it->rect.contains(event.position) || !contains(it->events, mask) ||
            !it->callback) {
            continue;
        }

        it->callback(event);
    }
}

bool InputRouter::dispatch_target(const InputEntry& target, UiEvent& event) {
    // run the entry callback, then bubble through parents.
    if (target.callback) target.callback(event);
    return dispatch(*target.node, event);
}
