#pragma once

#include "event.hpp"
#include "../constants.hpp"
#include "../layout/geometry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace ui {
    class Debugger;
    class Node;

    enum class RegionKind : unsigned char {
        Target,
        Blocker,
        Observer,
    };

    struct RegionConfig {
        /// manual router calls read this rect in screen coordinates; node calls offset it from the node's top-left.
        Rect rect{};

        /// defaults to pointer events; this config never gives keyboard focus to a node.
        EventMask events = EventMask::Pointer;

        /// runs after this target, blocker, or observer matches the event position and type.
        std::function<void(UiEvent&)> on_event;

        /// a larger value selects this target before overlapping targets with lower values.
        int priority = 0;
    };

    struct Region {
        /// null targets absorb matching events; other targets dispatch to this node.
        Node* node = nullptr;

        /// descendants of a blocker owner remain eligible targets.
        Node* owner = nullptr;

        /// screen-space bounds used by hit testing.
        Rect rect;

        /// event families accepted by this region.
        EventMask events = EventMask::Pointer;

        /// runs when this region wins hit testing.
        std::function<void(UiEvent&)> on_event;

        RegionKind kind = RegionKind::Target;
        int priority = 0;
    };

    inline constexpr std::size_t POINTER_BUTTON_COUNT = 3;

    struct InputRouterStats {
        std::size_t region_count = 0;
        std::size_t hit_test_count = 0;
        std::size_t region_checks = 0;
    };

    class InputRouter {
    public:
        /// removes every manual registration from the last frame; call register_* again before dispatching input.
        void begin_frame();

        /// advanced: sends input inside config.rect to node; normal widgets call Node::set_input_target().
        void register_region(Node& node, RegionConfig config);

        /// advanced: consumes matching pointer events inside config.rect without dispatching to a node.
        void register_blocker(RegionConfig config);

        /// consumes pointer events inside rect without dispatching them to a node.
        void register_blocker(Rect rect);

        /// advanced: consumes matching pointer events unless their target is owner or one of its descendants.
        void register_blocker(Node& owner, RegionConfig config);

        /// advanced: runs config.on_event for matching clicks without changing the selected input target.
        void register_observer(Node& owner, RegionConfig config);

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

        /// returns debug-only region and hit-test counters accumulated since begin_frame().
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

        void clear_region(Node& node);
        void clear_regions(Node& subtree);
        void clear_inactive_targets();
        void refresh_pointer_state(ImVec2 position);
        void set_input_flag(Node*& current, Node* next, InputFlag flag);
        const Region* pointer_target(UiEvent& event, bool& blocked);
        const Region* target_at(ImVec2 position, bool include_non_input, EventType type = EventType::PointerMove) const;
        const Region* blocking_region_at(ImVec2 position, EventType type, const Node* target = nullptr) const;
        void notify_observers(UiEvent& event);
        bool dispatch_target(const Region& target, UiEvent& event);

        std::vector<Region> m_regions;
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
    };

} // namespace ui
