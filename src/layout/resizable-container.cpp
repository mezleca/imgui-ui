#include <imgui-ui/layout/resizable-container.hpp>

#include <imgui-ui/imgui/draw.hpp>

#include <algorithm>

static constexpr float MIN_CHILD_SIZE = 32.0F;
static constexpr float CHILD_RESIZE_HANDLE_SIZE = 10.0F;
static constexpr float CHILD_RESIZE_HANDLE_INSET = 1.0F;

using namespace ui;

ResizableContainer::ResizableContainer(std::string id) : Container(std::move(id)) {
    set_type_name("ResizableContainer");
}

ResizableContainer& ResizableContainer::set_resize(ResizeAxes resize) {
    if (m_resize == resize) {
        return *this;
    }

    m_resize = resize;
    set_input_mode(resize == ResizeAxes::None ? InputMode::None : InputMode::Target);
    return *this;
}

ImGuiMouseCursor ResizableContainer::resize_cursor() const {
    if (m_resize == ResizeAxes::X) return ImGuiMouseCursor_ResizeEW;
    if (m_resize == ResizeAxes::Y) return ImGuiMouseCursor_ResizeNS;
    if (m_resize == ResizeAxes::Both) return ImGuiMouseCursor_ResizeNWSE;
    return ImGuiMouseCursor_Arrow;
}

void ResizableContainer::on_draw_end() {
    set_visual_rect(Rect::from_position_size(ImGui::GetWindowPos(), ImGui::GetWindowSize()));
    draw_resize_indicator();
    Container::on_draw_end();

    if (m_dragging) {
        ImGui::SetMouseCursor(resize_cursor());
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

void ResizableContainer::mouse_release_event(UiEvent& event) {
    if (event.button != PointerButton::Left || !m_dragging) return;

    m_dragging = false;
    m_resizing = ResizeAxes::None;
    release_pointer();
    event.block_native_input();
    event.stop_propagation();
}

void ResizableContainer::mouse_press_event(UiEvent& event) {
    if (m_resize == ResizeAxes::None || event.button != PointerButton::Left || !resize_handle().contains(event.position)) return;

    // capture the drag before recording its origin. later motion uses this pointer position and size.
    m_dragging = capture_pointer();
    if (!m_dragging) return;

    m_drag_start = event.position;
    m_previous_size = layout().size();
    m_resizing = m_resize;
    event.prevent_default();
    event.block_native_input();
    event.stop_propagation();
}

void ResizableContainer::mouse_move_event(UiEvent& event) {
    if (!m_dragging) return;

    // clamp the dragged size to the parent content bounds and the minimum widget size.
    const ImVec2 child_min = layout().visual_rect().min;
    const ImVec2 max_size = {
        std::max(MIN_CHILD_SIZE, m_parent_content_max.x - child_min.x),
        std::max(MIN_CHILD_SIZE, m_parent_content_max.y - child_min.y),
    };

    LayoutSize updated = layout().size_spec();
    if ((m_resizing & ResizeAxes::X) != ResizeAxes::None) {
        updated.width = px(std::clamp(m_previous_size.x + event.position.x - m_drag_start.x, MIN_CHILD_SIZE, max_size.x));
    }

    if ((m_resizing & ResizeAxes::Y) != ResizeAxes::None) {
        updated.height = px(std::clamp(m_previous_size.y + event.position.y - m_drag_start.y, MIN_CHILD_SIZE, max_size.y));
    }

    set_size(updated);
    event.block_native_input();
    event.stop_propagation();
}

void ResizableContainer::draw_resize_indicator() {
    if (m_resize == ResizeAxes::None) {
        return;
    }

    const ComputedStyle& current_style = computed_style();
    const float border_thickness = current_style.border_thickness();
    ImDrawList& window_draw_list = draw_list(DrawListTarget::Window);
    const ImVec2 max = resize_handle().max;

    for (int i = 0; i < 3; ++i) {
        const float distance = 3.0F + (static_cast<float>(i) * 4.0F);
        draw_line(
            window_draw_list, {max.x - distance - 1.0f, max.y}, {max.x, max.y - distance}, current_style.border_color().value,
            border_thickness
        );
        draw_line(
            window_draw_list, {max.x - distance + border_thickness + 0.5f, max.y},
            {max.x, max.y - distance + border_thickness + 0.5f}, current_style.background_color().value, border_thickness
        );
    }
}
