#pragma once

#include "../widgets/widget.hpp"

#include <imgui.h>
#include <string>
#include <string_view>

namespace ui {
    /// arranges children without owning pointer input; call set_input_target() when this container handles events.
    class ChildContainer : public Widget {
    public:
        explicit ChildContainer(std::string id, std::string_view type_name = "ChildContainer");

        /// enables scrolling while preserving the container's outer bounds.
        ChildContainer& set_scrollable(bool scrollable);
        /// replaces vertical style padding so one text line is centered in fixed height.
        ChildContainer& set_center_content_vertically(bool enabled);

    protected:
        /// opens the imgui child scope; on_draw_end() records its bounds and closes it after child nodes draw.
        bool paint_content() override;
        void on_layout() override;

        /// records the outer rectangle and closes the ImGui child window.
        void on_draw_end() override;

        virtual bool accepts_imgui_input() const {
            return true;
        }

    private:
        bool m_scrollable = false;
        bool m_center_content_vertically = false;
        Rect m_child_rect{};
    };
} // namespace ui
