#include "router.hpp"
#include "../tree/node.hpp"

#include <utility>

using namespace ui;

static bool is_pointer_event(EventType type) {
    return contains(EventMask::Pointer, event_mask(type));
}

static bool belongs_to(const Node* node, const Node* ancestor) {
    for (const Node* current = node; current != nullptr; current = current->parent()) {
        if (current == ancestor) {
            return true;
        }
    }

    return false;
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
    m_regions.clear();
    m_has_blockers = false;
    m_has_observers = false;

    if constexpr (constants::IS_DEBUG_BUILD) m_stats = {};
    clear_inactive_targets();
    if (m_debug_inspect_release_pending && !ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
        !ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        m_debug_inspect_release_pending = false;
    }
}

void InputRouter::set_debug_inspect_mode(bool enabled) {
    m_debug_inspect_mode = enabled;
    if (enabled) m_debug_inspect_release_pending = false;
}

void InputRouter::finish_debug_inspect_mode() {
    m_debug_inspect_mode = false;
    m_debug_inspect_release_pending = true;
}

void InputRouter::clear_debug_inspect_mode() {
    m_debug_inspect_mode = false;
    m_debug_inspect_release_pending = false;
}

void InputRouter::clear_region(Node& node) {
    std::erase_if(m_regions, [&node](const Region& region) { return region.node == &node || region.owner == &node; });
}

void InputRouter::clear_regions(Node& subtree) {
    std::erase_if(m_regions, [&subtree](const Region& region) {
        return belongs_to(region.node, &subtree) || belongs_to(region.owner, &subtree);
    });
    if (subtree.contains(m_hovered_node)) set_input_flag(m_hovered_node, nullptr, InputFlag::Hovered);
    if (subtree.contains(m_active_node)) set_input_flag(m_active_node, nullptr, InputFlag::Active);
}

void InputRouter::refresh_pointer_state(ImVec2 position) {
    const Region* target = target_at(position, false, EventType::PointerMove);
    if (m_has_blockers &&
        blocking_region_at(position, EventType::PointerMove, target == nullptr ? nullptr : target->node) != nullptr) {
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

void InputRouter::register_region(Node& node, RegionConfig config) {
    m_regions.push_back(
        Region{&node, nullptr, config.rect, config.events, std::move(config.on_event), RegionKind::Target, config.priority}
    );
}

void InputRouter::register_blocker(RegionConfig config) {
    m_has_blockers = true;
    m_regions.push_back(
        Region{nullptr, nullptr, config.rect, config.events, std::move(config.on_event), RegionKind::Blocker, config.priority}
    );
}

void InputRouter::register_blocker(Rect rect) {
    RegionConfig config;
    config.rect = rect;
    register_blocker(std::move(config));
}

void InputRouter::register_blocker(Node& owner, RegionConfig config) {
    m_has_blockers = true;
    m_regions.push_back(
        Region{nullptr, &owner, config.rect, config.events, std::move(config.on_event), RegionKind::Blocker, config.priority}
    );
}

void InputRouter::register_observer(Node& owner, RegionConfig config) {
    m_has_observers = true;
    m_regions.push_back(
        Region{nullptr, &owner, config.rect, config.events, std::move(config.on_event), RegionKind::Observer, config.priority}
    );
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
    // replace the keyboard target and notify both nodes in focus transition order.
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
    // discard focus, capture, hover, and presses whose nodes became unavailable before routing this event.
    clear_inactive_targets();

    // debugger selection consumes application input until its pending release has completed.
    if (debug_inspect_mode()) {
        event.mark_handled();
        return true;
    }

    // keyboard and text input bypass hit testing and bubble from the focused node.
    if (is_keyboard_event(event.type)) {
        if (m_focused_node == nullptr) {
            return false;
        }

        return dispatch(*m_focused_node, event);
    }

    if (!is_pointer_event(event.type)) {
        return false;
    }

    // native click events are uncommon, but observers must receive them just like synthesized clicks.
    if (event.type == EventType::Click || event.type == EventType::ContextClick) {
        notify_observers(event);
    }

    // captured moves stay with the drag origin even after the pointer leaves its region.
    if (event.type == EventType::PointerMove) {
        if (m_pointer_capture != nullptr) {
            return dispatch(*m_pointer_capture, event);
        }

        bool blocked = false;
        const Region* target = pointer_target(event, blocked);

        if (blocked) return true;
        if (target != nullptr) return dispatch_target(*target, event);

        return false;
    }

    // send release to a drag first, then release capture unless the handler replaced it.
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

        // consume the press record so one release can synthesize at most one click.
        const PressedPointer pressed = std::exchange(m_pressed[*button_index], {});
        const bool default_prevented = pressed.prevent_click || event.default_prevented;

        bool blocked = false;
        const Region* released = pointer_target(event, blocked);
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

    // resolve hover, target, and blockers once before dispatching down or scroll input.
    bool blocked = false;
    const Region* target = pointer_target(event, blocked);
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

    // retain the pressed target so a later release can create a click only on the same node.
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
    const Region* target = target_at(position, false);
    return target == nullptr ? nullptr : target->node;
}

InputRouterStats InputRouter::stats() const {
    if constexpr (!constants::IS_DEBUG_BUILD) return {};

    InputRouterStats result = m_stats;
    result.region_count = m_regions.size();
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

const Region* InputRouter::pointer_target(UiEvent& event, bool& blocked) {
    // blockers consume matching pointer input before the target receives it and clear hover behind them.
    const Region* target = target_at(event.position, false, event.type);
    if (m_has_blockers) {
        const Region* blocker = blocking_region_at(event.position, event.type, target == nullptr ? nullptr : target->node);
        if (blocker != nullptr) {
            set_input_flag(m_hovered_node, nullptr, InputFlag::Hovered);
            if (blocker->on_event) {
                blocker->on_event(event);
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

const Region* InputRouter::target_at(ImVec2 position, bool include_non_input, EventType type) const {
    const Region* target = nullptr;
    if constexpr (constants::IS_DEBUG_BUILD) ++m_stats.hit_test_count;

    for (auto it = m_regions.rbegin(); it != m_regions.rend(); ++it) {
        if constexpr (constants::IS_DEBUG_BUILD) ++m_stats.region_checks;

        if (it->kind != RegionKind::Target || it->node == nullptr || !it->rect.contains(position) ||
            !contains(it->events, event_mask(type))) {
            continue;
        }

        if (!it->node->visible() || (!include_non_input && !it->node->accepts_input())) {
            continue;
        }

        if (target == nullptr || it->priority > target->priority) {
            target = &*it;
            continue;
        }

        if (it->priority < target->priority) continue;

        // node::draw registers parents after children, so a child must win
        // even when its region was registered earlier.
        if (belongs_to(it->node, target->node)) {
            target = &*it;
        }
    }

    return target;
}

const Region* InputRouter::blocking_region_at(ImVec2 position, EventType type, const Node* target) const {
    const EventMask mask = event_mask(type);
    if (mask == EventMask::None) {
        return nullptr;
    }

    if constexpr (constants::IS_DEBUG_BUILD) ++m_stats.hit_test_count;

    for (auto it = m_regions.rbegin(); it != m_regions.rend(); ++it) {
        if constexpr (constants::IS_DEBUG_BUILD) ++m_stats.region_checks;
        if (it->kind == RegionKind::Blocker && it->rect.contains(position) && contains(it->events, mask) &&
            (it->owner == nullptr || (it->owner->visible() && it->owner->enabled() && !belongs_to(target, it->owner)))) {
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
    for (auto it = m_regions.rbegin(); it != m_regions.rend(); ++it) {
        if constexpr (constants::IS_DEBUG_BUILD) ++m_stats.region_checks;
        if (it->kind != RegionKind::Observer || !it->rect.contains(event.position) || !contains(it->events, mask) ||
            it->owner == nullptr || !it->owner->visible() || !it->owner->enabled() || !it->on_event) {
            continue;
        }

        it->on_event(event);
    }
}

bool InputRouter::dispatch_target(const Region& target, UiEvent& event) {
    // run the region callback before node behavior, then bubble through the node's parents.
    if (target.on_event) target.on_event(event);
    return dispatch(*target.node, event);
}
