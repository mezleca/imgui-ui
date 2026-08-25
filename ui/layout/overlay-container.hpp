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
            return false;
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
