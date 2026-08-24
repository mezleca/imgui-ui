#pragma once

#include "../style/styled-node.hpp"
#include "../imgui/input-bridge.hpp"

#include <functional>
#include <string>
#include <string_view>

namespace ui {
    class Widget : public StyledNode {
    public:
        explicit Widget(std::string id, std::string_view type_name = "Widget") : StyledNode(std::move(id), type_name) {}

        Widget& set_font(ImFont* font) override {
            StyledNode::set_font(font);
            return *this;
        }

        /// receives events after the widget's internal behavior has run.
        std::function<void(UiEvent&)> on_event;

        bool accepts_input() const override {
            return Node::accepts_input() && accepts_visual_input();
        }

        /// converts an input snapshot into active/focus/hover style selection.
        void apply_input_state(const ItemInputState& input, bool active = false) {
            set_interaction_style(input.hovered, input.active || active, input.focused);
        }

    protected:
        void dispatch_event(UiEvent& event) override {
            Node::dispatch_event(event);
            if (on_event) {
                on_event(event);
            }
        }

    private:
        float draw_opacity() const override {
            return opacity();
        }
    };

} // namespace ui
