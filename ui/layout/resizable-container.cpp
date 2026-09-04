#include "resizable-container.hpp"

#include "../imgui/draw.hpp"

#include <algorithm>

static constexpr float MIN_CHILD_SIZE = 32.0F;
static constexpr float CHILD_RESIZE_HANDLE_SIZE = 10.0F;
static constexpr float CHILD_RESIZE_HANDLE_INSET = 1.0F;

using namespace ui;

ResizableContainer::ResizableContainer(std::string id) : StackContainer(std::move(id)) {
    set_type_name("ResizableContainer");
    set_input_target();
    _on_event = [this](UiEvent& event) { handle_resize(event); };
}

ResizableContainer& ResizableContainer::set_resize(ResizeAxes resize) {
    m_resize = resize;
    return *this;
}

ImGuiMouseCursor ResizableContainer::resize_cursor() const {
    if (m_resize == ResizeAxes::X) return ImGuiMouseCursor_ResizeEW;
    if (m_resize == ResizeAxes::Y) return ImGuiMouseCursor_ResizeNS;
    if (m_resize == ResizeAxes::Both) return ImGuiMouseCursor_ResizeNWSE;
    return ImGuiMouseCursor_Arrow;
}

void ResizableContainer::update_resize_cursor() const {
    ImGui::SetMouseCursor(resize_cursor());
}

void ResizableContainer::on_draw_end() {
    set_visual_rect(Rect::from_position_size(ImGui::GetWindowPos(), ImGui::GetWindowSize()));
    draw_resize_indicator();
    StackContainer::on_draw_end();

    if (m_dragging) {
        update_resize_cursor();
    }

    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const ImVec2 available = ImGui::GetContentRegionAvail();
    m_parent_content_max = {cursor.x + available.x, cursor.y + available.y};
}

Rect ResizableContainer::hit_rect(Rect visual_rect) const {
    if (m_resize == ResizeAxes::None) {
        return visual_rect;
    }

    return resize_handle();
}

Rect ResizableContainer::resize_handle() const {
    const ImVec2 max = layout().visual_rect().max;
    return Rect::from_position_size(
        {max.x - CHILD_RESIZE_HANDLE_SIZE - CHILD_RESIZE_HANDLE_INSET,
         max.y - CHILD_RESIZE_HANDLE_SIZE - CHILD_RESIZE_HANDLE_INSET},
        {CHILD_RESIZE_HANDLE_SIZE, CHILD_RESIZE_HANDLE_SIZE}
    );
}

void ResizableContainer::handle_resize(UiEvent& event) {
    if (m_resize == ResizeAxes::None) {
        return;
    }

    if (event.type == EventType::PointerUp && event.button == PointerButton::Left && m_dragging) {
        m_dragging = false;
        m_resizing = ResizeAxes::None;
        release_pointer();
        event.stop_propagation();
        return;
    }

    if (event.type == EventType::PointerDown && event.button == PointerButton::Left && resize_handle().contains(event.position)) {
        // capture only after the pointer enters the handle; later moves use the original size and pointer position.
        m_dragging = capture_pointer();
        if (!m_dragging) {
            return;
        }

        m_drag_start = event.position;
        m_previous_size = layout().size();
        m_resizing = m_resize;
        event.prevent_default();
        event.stop_propagation();
        return;
    }

    if (event.type != EventType::PointerMove || !m_dragging) {
        return;
    }

    // clamp the dragged size to the parent content bounds and the minimum widget size.
    const ImVec2 child_min = layout().visual_rect().min;
    const ImVec2 max_size = {
        std::max(MIN_CHILD_SIZE, m_parent_content_max.x - child_min.x),
        std::max(MIN_CHILD_SIZE, m_parent_content_max.y - child_min.y),
    };

    ImVec2 size = layout().size();

    if ((m_resizing & ResizeAxes::X) != ResizeAxes::None) {
        size.x = std::clamp(m_previous_size.x + event.position.x - m_drag_start.x, MIN_CHILD_SIZE, max_size.x);
    }

    if ((m_resizing & ResizeAxes::Y) != ResizeAxes::None) {
        size.y = std::clamp(m_previous_size.y + event.position.y - m_drag_start.y, MIN_CHILD_SIZE, max_size.y);
    }

    set_size({px(size.x), px(size.y)});
    event.stop_propagation();
}

void ResizableContainer::draw_resize_indicator() {
    if (m_resize == ResizeAxes::None) {
        return;
    }

    const float border_thickness = style().border_thickness();
    ImDrawList& foreground_draw_list = draw_list(DrawListTarget::Foreground);
    const ImVec2 max = resize_handle().max;

    for (int i = 0; i < 3; ++i) {
        const float distance = 3.0F + static_cast<float>(i) * 4.0F;
        draw_line(
            foreground_draw_list, {max.x - distance - 1.0f, max.y}, {max.x, max.y - distance}, ImColor(160, 160, 160, 255),
            border_thickness
        );
        draw_line(
            foreground_draw_list, {max.x - distance + border_thickness + 0.5f, max.y},
            {max.x, max.y - distance + border_thickness + 0.5f}, ImColor{20, 20, 20, 255}, border_thickness
        );
    }
}
