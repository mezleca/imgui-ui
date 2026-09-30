#pragma once

#include <imgui.h>

#include <cstdint>
#include <functional>
#include <string>

namespace ui {
    class Node;
    struct UiEvent;

    using InputCallback = std::function<void(UiEvent&)>;

    enum class EventType : uint8_t {
        PointerMove,
        PointerDown,
        PointerUp,
        Scroll,
        KeyDown,
        KeyUp,
        /// a matching pointer press and release occurred on one target.
        Click,
        /// a matching secondary pointer press and release occurred on one target.
        ContextClick,
        TextInput,
        FocusGained,
        FocusLost,
        Cancel,
    };

    enum class EventMask : uint16_t {
        None = 0,
        PointerMove = 1 << 0,
        PointerDown = 1 << 1,
        PointerUp = 1 << 2,
        Scroll = 1 << 3,
        KeyDown = 1 << 4,
        KeyUp = 1 << 5,
        Click = 1 << 6,
        ContextClick = 1 << 7,
        TextInput = 1 << 8,
        FocusGained = 1 << 9,
        FocusLost = 1 << 10,
        Cancel = 1 << 11,
        Pointer = PointerMove | PointerDown | PointerUp | Click | ContextClick | Scroll,
        Keyboard = KeyDown | KeyUp | TextInput | Cancel,
        All = Pointer | Keyboard | FocusGained | FocusLost,
    };

    constexpr EventMask operator|(EventMask left, EventMask right) {
        return static_cast<EventMask>(static_cast<uint16_t>(left) | static_cast<uint16_t>(right));
    }

    constexpr EventMask operator&(EventMask left, EventMask right) {
        return static_cast<EventMask>(static_cast<uint16_t>(left) & static_cast<uint16_t>(right));
    }

    constexpr bool contains(EventMask mask, EventMask value) {
        return (mask & value) == value;
    }

    constexpr EventMask event_mask(EventType type) {
        const auto index = static_cast<uint8_t>(type);
        return index <= static_cast<uint8_t>(EventType::Cancel) ? static_cast<EventMask>(1U << index) : EventMask::None;
    }

    enum class PointerButton : uint8_t {
        None,
        Left,
        Right,
        Middle,
    };

    enum class Key : uint8_t {
        Unknown,
        Escape,
        Enter,
        Tab,
        Left,
        Right,
        Up,
        Down,
    };

    struct UiEvent {
        /// target of the current branch. stays unchanged while bubbling through parents, then changes for underlying targets.
        Node* target = nullptr;

        ImVec2 position;
        ImVec2 scroll;

        std::string text;

        /// the target or one of its ancestors consumed the event.
        bool handled = false;

        /// remaining handlers, ancestors, and underlying targets no longer receive the event.
        bool propagation_stopped = false;

        /// pointer release will not synthesize a click from this press.
        bool default_prevented = false;

        /// platform backends must not forward this event to imgui.
        bool native_input_blocked = false;

        EventType type;
        PointerButton button = PointerButton::None;
        Key key = Key::Unknown;

        static UiEvent make(EventType type) {
            UiEvent event;
            event.type = type;
            return event;
        }

        /// consumes the event without stopping its parent traversal.
        void mark_handled() {
            handled = true;
        }

        /// consumes the event and stops the remaining handlers and routing branches.
        void stop_propagation() {
            handled = true;
            propagation_stopped = true;
        }

        /// prevents the router's default action for this event.
        void prevent_default() {
            default_prevented = true;
        }

        void block_native_input() {
            native_input_blocked = true;
        }
    };

} // namespace ui
