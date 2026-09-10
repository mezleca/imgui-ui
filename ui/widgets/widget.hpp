#pragma once

#include "../style/styled-node.hpp"

#include <functional>
#include <imgui.h>
#include <string>
#include <string_view>

namespace ui {
    class Widget : public StyledNode {
    public:
        explicit Widget(std::string id, std::string_view type_name = "Widget", InputMode input_mode = InputMode::Target)
            : StyledNode(std::move(id), type_name) {
            set_input_mode(input_mode);
        }

        /// runs after internal event handling for every event reaching this widget.
        Widget& set_on_event(std::function<void(UiEvent&)> callback) {
            m_on_event = std::move(callback);
            return *this;
        }

        /// runs after the widget reports a value change.
        Widget& set_on_change(std::function<void()> callback) {
            m_on_change = std::move(callback);
            return *this;
        }

        bool accepts_input() const override {
            return Node::accepts_input() && accepts_visual_input();
        }

    protected:
        std::function<void(UiEvent&)> m_on_event;
        std::function<void()> m_on_change;

        void notify_change() {
            if (m_on_change) {
                m_on_change();
            }
        }

        void dispatch_event(UiEvent& event) override {
            Node::dispatch_event(event);
            if (m_on_event) {
                m_on_event(event);
            }

            if (event.type == EventType::Click) {
                on_click(event);
            }
        }

        virtual void on_click(UiEvent&) {}
    };

    class DrawListWidget : public Widget {
    public:
        explicit DrawListWidget(
            std::string id = {}, std::string_view type_name = "DrawListWidget", InputMode input_mode = InputMode::Target
        )
            : Widget(std::move(id), type_name, input_mode) {}

    private:
        bool paint() override {
            const Rect rect = Rect::from_position_size(ImGui::GetCursorScreenPos(), layout().size());
            ImGui::Dummy(rect.size());
            paint_draw_list(*ImGui::GetWindowDrawList(), rect, computed_style());
            return true;
        }

        virtual void paint_draw_list(ImDrawList& draw_list, Rect rect, const ComputedStyle& style) = 0;
    };
} // namespace ui
