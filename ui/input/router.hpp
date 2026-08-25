#pragma once

#include "event.hpp"
#include "layer.hpp"
#include "../layout/geometry.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace ui {
    class Node;

    enum class InputPolicy : unsigned char {
        /// the layer does not affect events below it.
        PassThrough,

        /// pointer events stop at this layer, but keyboard focus below remains usable.
        BlockPointer,

        /// pointer and keyboard events stop at this layer.
        BlockAll,
    };

    struct Region {
        /// node receiving an event when this region wins hit testing.
        Node* node;

        /// screen-space bounds used by hit testing.
        Rect rect;

        /// layer used to apply input policies.
        InputLayer layer;
    };

    inline constexpr std::size_t LAYER_COUNT = static_cast<std::size_t>(InputLayer::Count);
    inline constexpr std::size_t POINTER_BUTTON_COUNT = 3;

    class InputRouter {
    public:
        /// clears regions from the previous draw pass and stale persistent targets.
        void begin_frame();

        /// blocks normal dispatch while the debugger selects a node.
        void set_debug_inspect_mode(bool enabled);

        /// keeps the next mouse release from reaching application nodes.
        void finish_debug_inspect_mode();

        /// stops debugger input suppression immediately.
        void clear_debug_inspect_mode();

        /// defines which events a layer prevents from reaching lower layers.
        void set_layer_policy(InputLayer layer, InputPolicy policy);

        /// reports whether pointer state for a layer is blocked by a higher layer.
        bool pointer_blocked_for(InputLayer layer) const;

        /// assigns the fallback keyboard target for the node's layer.
        void set_keyboard_target(Node& node);

        /// removes the keyboard fallback for a layer.
        void clear_keyboard_target(InputLayer layer);

        /// removes keyboard fallbacks pointing into a subtree.
        void clear_keyboard_target(Node& subtree);

        /// adds a screen-space hit-test region using the node's assigned layer.
        void register_region(Node& node, Rect rect);

        /// adds a region with an explicit layer without changing the node tree.
        void register_region_in_layer(Node& node, Rect rect, InputLayer layer);

        /// captures subsequent pointer moves and releases for a node.
        bool capture_pointer(Node& node);

        /// releases the current pointer capture.
        void release_pointer();

        /// releases pointer state pointing into a subtree.
        void release_pointer(Node& subtree);

        /// gives keyboard events to a visible input node in its assigned layer.
        bool set_focus(Node& node);

        /// gives keyboard events to a node in an explicit layer.
        bool set_focus_in_layer(Node& node, InputLayer layer);

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

        /// reports whether normal event dispatch is currently suppressed.
        bool debug_inspect_mode() const {
            return m_debug_inspect_mode || m_debug_inspect_release_pending;
        }

        /// resolves and dispatches a platform event.
        bool dispatch(UiEvent& event);

        /// dispatches directly to a node, then bubbles through its parents.
        bool dispatch(Node& target, UiEvent& event);

        /// returns the normal input node at a screen position.
        Node* node_at(ImVec2 position) const;

        /// returns a visible node at a position, including non-interactive nodes.
        Node* debug_node_at(ImVec2 position) const;

    private:
        friend class Node;

        void clear_regions(Node& subtree);
        void clear_inactive_targets();
        Node* target_at(ImVec2 position, InputLayer minimum_layer, bool include_non_input) const;
        Node* topmost_keyboard_target_from(std::size_t minimum_index) const;
        std::optional<InputLayer> highest_blocking_layer(EventType type) const;

        std::vector<Region> m_regions;
        std::array<InputPolicy, LAYER_COUNT> m_policies{};
        Node* m_focused_node = nullptr;
        InputLayer m_focused_layer = InputLayer::Content;
        bool m_debug_inspect_mode = false;
        bool m_debug_inspect_release_pending = false;
        std::array<Node*, LAYER_COUNT> m_keyboard_targets{};
        Node* m_pointer_capture = nullptr;
        std::array<Node*, POINTER_BUTTON_COUNT> m_pressed_targets{};
        std::array<bool, POINTER_BUTTON_COUNT> m_pressed_default_prevented{};
    };

} // namespace ui
