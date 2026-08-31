#pragma once

#include "../tree/node.hpp"

namespace ui {
    enum class OverlayPosition {
        LEFT,
        RIGHT,
    };

    class OverlayNode : public Node {
    public:
        explicit OverlayNode(std::string id) : Node(std::move(id)) {}

        /// blocks pointer events outside this overlay's children when blocks is true.
        OverlayNode& set_blocks_pointer_input(bool blocks) {
            if (blocks)
                set_input_blocker();
            else
                clear_input_target();
            return *this;
        }

    protected:
        void on_layout() override {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            resolve_size(viewport->WorkSize);
        }

        bool on_draw() override {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            set_screen_rect(Rect::from_position_size(viewport->WorkPos, viewport->WorkSize));
            return true;
        }
    };
} // namespace ui
