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

        bool accepts_input() const override {
            return m_blocks_pointer_input && Node::accepts_input();
        }

        /// makes this full-viewport overlay consume pointer input outside its children.
        OverlayNode& set_blocks_pointer_input(bool blocks) {
            m_blocks_pointer_input = blocks;
            update_pointer_blocker_registration();
            return *this;
        }

        bool blocks_pointer_input() const override {
            return m_blocks_pointer_input;
        }

        Rect pointer_blocking_rect() const override {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            return Rect::from_position_size(viewport->WorkPos, viewport->WorkSize);
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

    private:
        bool m_blocks_pointer_input = false;
    };
} // namespace ui
