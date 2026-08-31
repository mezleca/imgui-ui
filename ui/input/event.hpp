#pragma once

#include <imgui.h>

#include <cstdint>
#include <string>

namespace ui {
    enum class EventType {
        PointerMove,
        PointerDown,
        PointerUp,
        Click,
        ContextClick,
        Scroll,
        KeyDown,
        KeyUp,
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
        Click = 1 << 3,
        ContextClick = 1 << 4,
        Scroll = 1 << 5,
        KeyDown = 1 << 6,
        KeyUp = 1 << 7,
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
        switch (type) {
            case EventType::PointerMove:
                return EventMask::PointerMove;
            case EventType::PointerDown:
                return EventMask::PointerDown;
            case EventType::PointerUp:
                return EventMask::PointerUp;
            case EventType::Click:
                return EventMask::Click;
            case EventType::ContextClick:
                return EventMask::ContextClick;
            case EventType::Scroll:
                return EventMask::Scroll;
            case EventType::KeyDown:
                return EventMask::KeyDown;
            case EventType::KeyUp:
                return EventMask::KeyUp;
            case EventType::TextInput:
                return EventMask::TextInput;
            case EventType::FocusGained:
                return EventMask::FocusGained;
            case EventType::FocusLost:
                return EventMask::FocusLost;
            case EventType::Cancel:
                return EventMask::Cancel;
        }

        return EventMask::None;
    }

    enum class PointerButton {
        None,
        Left,
        Right,
        Middle,
    };

    enum class Key {
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
        ImVec2 position{};
        ImVec2 scroll{};

        std::string text;

        /// the target or one of its ancestors consumed the event.
        bool handled = false;

        /// parent nodes no longer receive the event.
        bool propagation_stopped = false;

        /// pointer release will not synthesize a click from this press.
        bool default_prevented = false;

        EventType type;
        PointerButton button = PointerButton::None;
        Key key = Key::Unknown;

        static UiEvent make(EventType type) {
            return {
                .position = {},
                .scroll = {},
                .text = {},
                .handled = false,
                .propagation_stopped = false,
                .default_prevented = false,
                .type = type,
                .button = PointerButton::None,
                .key = Key::Unknown,
            };
        }

        /// consumes the event without stopping its parent traversal.
        void mark_handled() {
            handled = true;
        }

        /// consumes the event and stops dispatching it to parent nodes.
        void stop_propagation() {
            handled = true;
            propagation_stopped = true;
        }

        /// prevents the router's default action for this event.
        void prevent_default() {
            default_prevented = true;
        }
    };

} // namespace ui
