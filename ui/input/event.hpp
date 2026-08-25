#pragma once

#include <imgui.h>

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
        EventType type;
        ImVec2 position{};
        ImVec2 scroll{};
        PointerButton button = PointerButton::None;
        Key key = Key::Unknown;
        std::string text;
        /// the target or one of its ancestors consumed the event.
        bool handled = false;

        /// parent nodes no longer receive the event.
        bool propagation_stopped = false;

        /// pointer release will not synthesize a click from this press.
        bool default_prevented = false;

        static UiEvent make(EventType type) {
            return {
                .type = type,
                .position = {},
                .scroll = {},
                .button = PointerButton::None,
                .key = Key::Unknown,
                .text = {},
                .handled = false,
                .propagation_stopped = false,
                .default_prevented = false,
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
