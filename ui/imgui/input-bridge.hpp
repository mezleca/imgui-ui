#pragma once

#include "../input/router.hpp"

namespace ui {
    struct ItemInputState {
        /// the mouse is inside the item's visible bounds.
        bool hovered = false;

        /// ImGui reports the item as pressed or held.
        bool active = false;

        /// the router currently assigns keyboard focus to the node.
        bool focused = false;
    };

    class ImGuiInputBridge {
    public:
        explicit ImGuiInputBridge(InputRouter& router) : m_router(router) {}

        /// records the current ImGui item and returns its interaction state.
        ItemInputState observe_item(Node& node);

    private:
        InputRouter& m_router;
    };
} // namespace ui
