#pragma once

#include "hit-test-index.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ui {
    class Node;
    class UI;

    inline constexpr std::size_t POINTER_BUTTON_COUNT = 3;

    struct InputRouterStats {
        /// entries registered after the most recent begin_frame().
        std::size_t entry_count = 0;
        /// entry inspections performed by target and blocker queries in this frame.
        std::size_t entry_checks = 0;
    };

    /// routes one ui surface's input through its retained node tree.
    ///
    /// hit regions are rebuilt after layout because their screen bounds are frame-local.
    /// focus, pointer capture, and pressed buttons are surface-wide state.
    class InputRouter {
    public:
        InputRouter() = default;
        ~InputRouter();
        InputRouter(const InputRouter&) = delete;
        InputRouter& operator=(const InputRouter&) = delete;

        /// starts a frame by clearing entries, callbacks, statistics, and stale input state.
        void begin_frame();

        /// adds a screen-space target that bypasses node input registration.
        void register_target(Node& node, Rect rect, InputCallback callback = {});

        /// consumes selected events in rect and prevents imgui from receiving them.
        void register_blocker(Rect rect, InputCallback callback = {}, EventMask events = EventMask::Pointer);

        /// consumes selected events outside the visible owner's input descendants.
        void register_blocker(Node& owner, Rect rect, InputCallback callback = {}, EventMask events = EventMask::Pointer);

        /// routes later pointer moves and releases to node until released.
        bool capture_pointer(Node& node);

        /// releases the current pointer capture.
        void release_pointer();

        /// releases capture and presses pointing into a subtree.
        void release_pointer(Node& subtree);

        /// gives keyboard focus to a visible, enabled node that accepts input.
        bool set_focus(Node& node);

        /// sends a focus-lost event and clears the current focus.
        void clear_focus();

        /// clears focus when it points into a subtree.
        void clear_focus(Node& subtree);

        /// moves focus from a subtree to its nearest active blocker ancestor.
        void restore_focus(Node& subtree);

        /// returns the node currently receiving keyboard focus, if any.
        Node* focused_node() {
            return m_focused_node;
        }

        /// returns the node currently receiving keyboard focus, if any.
        const Node* focused_node() const {
            return m_focused_node;
        }

        /// routes one event to its focused, captured, or hit-tested node.
        bool dispatch(UiEvent& event);

        /// dispatches to target, then walks its parent chain until propagation stops.
        static bool dispatch(Node& target, UiEvent& event);

        /// returns the eligible target at position without applying blockers.
        Node* node_at(ImVec2 position) const;

        /// returns current entry count and scans performed since begin_frame().
        InputRouterStats stats() const;

    private:
        friend class Node;
        friend class UI;

        enum class InputFlag : uint8_t {
            Hovered,
            Active,
            Focused,
        };

        /// preserves press state until the matching release decides click synthesis.
        struct PressedPointer {
            Node* target = nullptr;
            /// suppresses a synthesized click after the press prevents its default action.
            bool prevent_click = false;
            /// repeats native input blocking on the matching pointer release.
            bool native_input_blocked = false;
        };

        /// blocks application dispatch while the debugger selects a node.
        void set_debug_inspect_mode(bool enabled);

        /// blocks application hover while the debugger owns the pointer.
        void set_debug_pointer_blocked(bool blocked);

        /// removes entries whose target or blocker owner is node.
        void erase_entries(Node& node);
        /// removes subtree entries and clears their hover and active flags.
        void clear_subtree_entries(Node& subtree);
        /// clears every router reference into subtree before it is detached.
        void detach(Node& subtree);
        /// records node so the destructor can detach this router from it.
        void attach_node(Node& node);
        /// removes node from the destructor's attachment list.
        void detach_node(Node& node);
        /// clears flag when current points into subtree.
        static void clear_input_flag(Node& subtree, Node*& current, InputFlag flag);
        /// resolves a node hit rect into one clipped screen-space entry.
        void register_node(Node& node, bool blocker, Rect input_rect, Rect visual_rect);
        /// clears focus, capture, hover, active, and press state for inactive nodes.
        void clear_inactive_targets();
        /// updates hover from a position without dispatching an event.
        void refresh_pointer_state(ImVec2 position);
        /// moves one input flag between nodes and resets the cursor when hover clears.
        static void set_input_flag(Node*& current, Node* next, InputFlag flag);
        /// resolves a pointer entry, dispatches a matching blocker, and updates hover.
        const HitTestIndex::Entry* pointer_target(UiEvent& event, bool& blocked);
        /// returns the target or blocker owner visible to debugger inspection.
        Node* inspect_node_at(ImVec2 position, EventType type) const;
        /// runs the entry callback before bubbling the event from its target node.
        bool dispatch_target(const HitTestIndex::Entry& target, UiEvent& event);
        static bool dispatch_bubble(Node& target, UiEvent& event);

        HitTestIndex m_hit_test;
        Node* m_focused_node = nullptr;
        bool m_debug_inspect_mode = false;
        bool m_debug_pointer_blocked = false;
        Node* m_pointer_capture = nullptr;
        Node* m_hovered_node = nullptr;
        Node* m_active_node = nullptr;
        std::array<PressedPointer, POINTER_BUTTON_COUNT> m_pressed{};
        std::vector<Node*> m_attached_nodes;
    };

} // namespace ui
