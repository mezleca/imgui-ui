#pragma once

#include "event.hpp"
#include "../constants.hpp"
#include "../layout/geometry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace ui {
    class Debugger;
    class Node;

    using InputCallback = std::function<void(UiEvent&)>;

    inline constexpr std::size_t POINTER_BUTTON_COUNT = 3;

    struct InputRouterStats {
        std::size_t entry_count = 0;
        std::size_t hit_test_count = 0;
        std::size_t entry_checks = 0;
    };

    class InputRouter {
    public:
        /// disconnects nodes before the router is destroyed.
        ~InputRouter();

        /// clears the transient input entries from the previous frame.
        void begin_frame();

        /// advanced target outside the node tree. normal widgets use Node::set_input_target().
        void target(Node& node, Rect rect, InputCallback callback = {});

        /// consumes events inside a screen-space rectangle for the current frame.
        void block(Rect rect, InputCallback callback = {}, EventMask events = EventMask::Pointer);

        /// consumes events outside owner's descendants while the owner remains visible.
        void block(Node& owner, Rect rect, InputCallback callback = {}, EventMask events = EventMask::Pointer);

        /// observes matching events without changing their target.
        void observe(Rect rect, InputCallback callback, EventMask events = EventMask::Pointer);

        /// captures subsequent pointer moves and releases for a node.
        bool capture_pointer(Node& node);

        /// releases the current pointer capture.
        void release_pointer();

        /// releases pointer state pointing into a subtree.
        void release_pointer(Node& subtree);

        /// gives keyboard events to a visible input node.
        bool set_focus(Node& node);

        /// sends a focus-lost event and clears the current focus.
        void clear_focus();

        /// clears focus when it points into a subtree.
        void clear_focus(Node& subtree);

        /// returns the node currently receiving keyboard focus, if any.
        Node* focused_node() {
            return m_focused_node;
        }

        const Node* focused_node() const {
            return m_focused_node;
        }

        /// resolves and dispatches a platform event.
        bool dispatch(UiEvent& event);

        /// dispatches directly to a node, then bubbles through its parents.
        bool dispatch(Node& target, UiEvent& event);

        /// returns the normal input node at a screen position.
        Node* node_at(ImVec2 position) const;

        /// returns debug-only entry and hit-test counters accumulated since begin_frame().
        InputRouterStats stats() const;

    private:
        friend class Node;
        friend class Debugger;

        enum class InputFlag {
            Hovered,
            Active,
            Focused,
        };

        struct PressedPointer {
            Node* target = nullptr;
            bool prevent_click = false;
        };

        /// blocks normal dispatch while the debugger selects a node.
        void set_debug_inspect_mode(bool enabled);

        /// keeps the next mouse release from reaching application nodes.
        void finish_debug_inspect_mode();

        /// stops debugger input suppression immediately.
        void clear_debug_inspect_mode();

        bool debug_inspect_mode() const {
            return m_debug_inspect_mode || m_debug_inspect_release_pending;
        }

        enum class InputKind : unsigned char {
            Target,
            Blocker,
            Observer,
        };

        struct InputEntry {
            Node* node = nullptr;
            Node* owner = nullptr;
            Rect rect;
            EventMask events = EventMask::Pointer;
            InputCallback callback;
            InputKind kind = InputKind::Target;
        };

        void erase_entries(Node& node);
        void clear_subtree_entries(Node& subtree);
        void detach(Node& subtree);
        void attach_node(Node& node);
        void detach_node(Node& node);
        void clear_input_flag(Node& subtree, Node*& current, InputFlag flag);
        void add_entry(Node* node, Node* owner, InputKind kind, Rect rect, EventMask events, InputCallback callback);
        void register_node(Node& node, bool blocker, Rect input_rect, Rect visual_rect);
        void clear_inactive_targets();
        void refresh_pointer_state(ImVec2 position);
        void set_input_flag(Node*& current, Node* next, InputFlag flag);
        const InputEntry* pointer_target(UiEvent& event, bool& blocked);
        const InputEntry* target_at(ImVec2 position, EventType type = EventType::PointerMove) const;
        const InputEntry* blocking_entry_at(ImVec2 position, EventType type, const Node* target = nullptr) const;
        void notify_observers(UiEvent& event);
        bool dispatch_target(const InputEntry& target, UiEvent& event);

        std::vector<InputEntry> m_entries;
        Node* m_focused_node = nullptr;
        bool m_debug_inspect_mode = false;
        bool m_debug_inspect_release_pending = false;
        Node* m_pointer_capture = nullptr;
        Node* m_hovered_node = nullptr;
        Node* m_active_node = nullptr;
        bool m_has_blockers = false;
        bool m_has_observers = false;
        std::array<PressedPointer, POINTER_BUTTON_COUNT> m_pressed{};
        mutable InputRouterStats m_stats;
        std::vector<Node*> m_attached_nodes;
    };

} // namespace ui
