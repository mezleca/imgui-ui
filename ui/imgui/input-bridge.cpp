#include "input-bridge.hpp"

#include "../tree/node.hpp"

namespace ui {
    ItemInputState ImGuiInputBridge::observe_item(Node& node) {
        ItemInputState state;
        const ImVec2 item_min = ImGui::GetItemRectMin();
        const ImVec2 item_max = ImGui::GetItemRectMax();
        const bool has_item_rect = item_max.x > item_min.x && item_max.y > item_min.y;

        if (node.accepts_input() && ImGui::IsItemVisible()) {
            m_router.register_region(node, {item_min, item_max});
        }

        if (m_router.debug_inspect_mode()) {
            return state;
        }

        const InputLayer node_layer = node.input_layer() == InputLayer::Count ? InputLayer::Content : node.input_layer();
        const bool pointer_blocked = m_router.pointer_blocked_for(node_layer);
        const bool popup_open = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup);
        state.hovered = !pointer_blocked && (ImGui::IsItemHovered() ||
                                             (!popup_open && has_item_rect && ImGui::IsMouseHoveringRect(item_min, item_max)));
        state.active = !pointer_blocked && ImGui::IsItemActive();

        if (state.active && node.accepts_input() && node.accepts_focus()) {
            state.focused = m_router.set_focus(node);
        } else if (!pointer_blocked && m_router.focused_node() == &node) {
            m_router.clear_focus();
        }

        return state;
    }

} // namespace ui
