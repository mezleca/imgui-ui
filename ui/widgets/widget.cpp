#include "widget.hpp"

using namespace ui;

Widget::Widget(std::string id, std::string_view type_name, InputMode input_mode) : StyledNode(std::move(id), type_name) {
    set_input_mode(input_mode);
}

Widget& Widget::on_event(InputCallback callback) {
    m_on_event = std::move(callback);

    if (m_on_event && !has_input_mode()) {
        set_input_mode(InputMode::Target);
    }

    return *this;
}

Widget& Widget::set_event_callback(EventType type, InputCallback callback) {
    if (!m_event_callbacks) {
        if (!callback) return *this;

        m_event_callbacks = std::make_unique<EventCallbacks>();
    }

    if (callback && ui::contains(EventMask::Pointer, event_mask(type)) && !has_input_mode()) {
        set_input_mode(InputMode::Target);
    }

    (*m_event_callbacks)[static_cast<std::size_t>(type)] = std::move(callback);
    return *this;
}

Widget& Widget::on_mouse_press(InputCallback callback) {
    return set_event_callback(EventType::PointerDown, std::move(callback));
}

Widget& Widget::on_mouse_release(InputCallback callback) {
    return set_event_callback(EventType::PointerUp, std::move(callback));
}

Widget& Widget::on_mouse_move(InputCallback callback) {
    return set_event_callback(EventType::PointerMove, std::move(callback));
}

Widget& Widget::on_wheel(InputCallback callback) {
    return set_event_callback(EventType::Scroll, std::move(callback));
}

Widget& Widget::on_key_press(InputCallback callback) {
    return set_event_callback(EventType::KeyDown, std::move(callback));
}

Widget& Widget::on_key_release(InputCallback callback) {
    return set_event_callback(EventType::KeyUp, std::move(callback));
}

Widget& Widget::on_change(std::function<void()> callback) {
    m_on_change = std::move(callback);
    return *this;
}

bool Widget::accepts_input() const {
    return Node::accepts_input() && accepts_visual_input();
}

void Widget::notify_change() {
    if (m_on_change) m_on_change();
}

void Widget::dispatch_event(UiEvent& event) {
    Node::dispatch_event(event);
    if (removal_pending() || event.propagation_stopped) return;

    // internal overrides run first, followed by the general callback, the specific callback, and the click hook.
    // removal or stopped propagation ends this sequence before the next handler.
    if (m_on_event) {
        m_on_event(event);
        if (removal_pending() || event.propagation_stopped) return;
    }

    const auto index = static_cast<std::size_t>(event.type);
    if (m_event_callbacks && index < m_event_callbacks->size() && (*m_event_callbacks)[index]) {
        (*m_event_callbacks)[index](event);
        if (removal_pending() || event.propagation_stopped) return;
    }

    if (event.type == EventType::Click) click_event(event);
}

bool DrawListWidget::paint() {
    const Rect rect = Rect::from_position_size(ImGui::GetCursorScreenPos(), layout().size());
    ImGui::Dummy(rect.size());
    ImDrawList& draw_list = *ImGui::GetWindowDrawList();
    const ComputedStyle& style = computed_style();
    draw_surface(draw_list, rect, style);
    paint_draw_list(draw_list, rect, style);
    return true;
}
