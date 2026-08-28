#include "child-container.hpp"

#include "../constants.hpp"
#include "../imgui/blur.hpp"
#include "../imgui/draw.hpp"
#include "../style/style.hpp"

namespace ui {
    ChildContainer::ChildContainer(std::string id, std::string_view type_name) : Widget(std::move(id), type_name) {
        const Theme theme = Theme::defaults();
        configure_all_styles([&theme](Style& style) {
            style.padding({theme.content_padding, theme.content_padding}).border_radius(theme.box_rounding);
        });
    }

    ChildContainer& ChildContainer::set_scrollable(bool scrollable) {
        m_scrollable = scrollable;
        return *this;
    }

    ChildContainer& ChildContainer::set_center_content_vertically(bool enabled) {
        m_center_content_vertically = enabled;
        return *this;
    }

    void ChildContainer::on_layout() {
        const ImVec2 size = requested_size();
        if (size_was_resolved() || !has_size_request() || size.x > 0.0F) {
            return;
        }

        resolve_size(resolve_layout_size(size, ImGui::GetContentRegionAvail()));
    }

    bool ChildContainer::on_draw() {
        ImGuiChildFlags child_flags = ImGuiChildFlags_AlwaysUseWindowPadding;
        ImGuiWindowFlags window_flags = constants::WIDGET_WINDOW_FLAGS;

        if (m_scrollable) {
            window_flags &= ~(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        }

        if (layout().size().x <= 0.0F) child_flags |= ImGuiChildFlags_AutoResizeX;
        if (layout().size().y <= 0.0F) child_flags |= ImGuiChildFlags_AutoResizeY;

        ImVec2 padding = style().padding();
        if (m_center_content_vertically && layout().size().y > 0.0F) {
            padding.y = std::max(0.0F, (layout().size().y - ImGui::GetFontSize()) * 0.5F);
        }
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);

        const ImVec2 position = ImGui::GetCursorScreenPos();
        draw_blur(
            {position, {position.x + layout().size().x, position.y + layout().size().y}}, style().blur(), style().border_radius(),
            style().alpha()
        );

        // beginchild copies window padding, border and background into the child
        // window padding can be restored while the child scope stays open.
        if (id().empty()) {
            ImGui::BeginChild(ImGui::GetID(this), layout().size(), child_flags, window_flags);
        } else {
            ImGui::BeginChild(id().c_str(), layout().size(), child_flags, window_flags);
        }

        ImGui::PopStyleVar();
        return true;
    }

    void ChildContainer::on_draw_end() {
        const ImVec2 window_position = ImGui::GetWindowPos();
        const ImVec2 window_size = ImGui::GetWindowSize();

        m_child_rect = Rect::from_position_size(window_position, window_size);
        // use the child window itself rather than its last item so padding,
        // scrolling and empty containers still have reliable outer bounds.
        set_screen_rect(m_child_rect);

        // draw on the child list before ending it; parent lists are composited first.
        draw_borders();

        ImGui::EndChild();
    }

    void ChildContainer::draw_borders() {
        const ImVec2& min = m_child_rect.min;
        const ImVec2& max = m_child_rect.max;

        const Style& current_style = style();
        if (current_style.border() == BORDER_NONE) {
            return;
        }

        draw_border({min, max}, current_style);
    }
} // namespace ui
