#pragma once

#include "../style/styled-node.hpp"

#include <functional>
#include <string>
#include <string_view>

namespace ui {
    class Widget : public StyledNode {
    public:
        explicit Widget(std::string id, std::string_view type_name = "Widget", bool input_target = true)
            : StyledNode(std::move(id), type_name) {
            if (input_target) {
                set_input_target();
            }
        }

        /// receives events after the widget's internal behavior has run.
        std::function<void(UiEvent&)> on_event;

        /// runs after this widget changes its bound value.
        std::function<void()> on_change;

        bool accepts_input() const override {
            return Node::accepts_input() && accepts_visual_input();
        }

    protected:
        void notify_change() {
            if (on_change) {
                on_change();
            }
        }

        void dispatch_event(UiEvent& event) override {
            Node::dispatch_event(event);
            if (on_event) {
                on_event(event);
            }
        }
    };

} // namespace ui
