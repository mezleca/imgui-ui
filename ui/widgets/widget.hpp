#pragma once

#include "../style/styled-node.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <imgui.h>
#include <memory>
#include <string>
#include <string_view>

namespace ui {
    enum class LabelPlacement : uint8_t {
        Inline,
        Above,
    };

    class Widget : public StyledNode {
    public:
        explicit Widget(std::string id, std::string_view type_name = "Widget", InputMode input_mode = InputMode::Target);

        /// runs after internal event handling for every event reaching this widget, unless propagation was stopped.
        /// a nonempty callback registers a pointer target when input mode is none. it does not consume events automatically.
        Widget& on_event(InputCallback callback);

        /// pointer callbacks register this widget as a target when its input mode is none. they do not block other targets.
        Widget& on_mouse_press(InputCallback callback);
        Widget& on_mouse_release(InputCallback callback);
        Widget& on_mouse_move(InputCallback callback);
        Widget& on_wheel(InputCallback callback);
        Widget& on_key_press(InputCallback callback);
        Widget& on_key_release(InputCallback callback);

        /// runs after the widget reports a value change.
        Widget& on_change(std::function<void()> callback);

        bool accepts_input() const override;

    protected:
        InputCallback m_on_event;
        std::function<void()> m_on_change;

        void notify_change();

        void dispatch_event(UiEvent& event) override;

        virtual void click_event(UiEvent&) {}

    private:
        using EventCallbacks = std::array<InputCallback, static_cast<std::size_t>(EventType::Click)>;

        Widget& set_event_callback(EventType type, InputCallback callback);
        std::unique_ptr<EventCallbacks> m_event_callbacks;
    };

    /// paints a widget directly into ImGui's draw list instead of emitting a native ImGui control.
    class DrawListWidget : public Widget {
    public:
        explicit DrawListWidget(
            std::string id = {}, std::string_view type_name = "DrawListWidget", InputMode input_mode = InputMode::Target
        )
            : Widget(std::move(id), type_name, input_mode) {}

    protected:
        virtual void draw_surface(ImDrawList& draw_list, Rect rect, const ComputedStyle&) const {
            StyledNode::draw_surface(draw_list, rect);
        }

    private:
        bool paint() override;

        virtual void paint_draw_list(ImDrawList&, Rect, const ComputedStyle&) {}
    };
} // namespace ui
