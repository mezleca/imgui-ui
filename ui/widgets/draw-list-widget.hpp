#pragma once

#include "widget.hpp"

namespace ui {
    class DrawListWidget : public Widget {
    public:
        explicit DrawListWidget(std::string id = {}, std::string_view type_name = "DrawListWidget", bool input_target = true)
            : Widget(std::move(id), type_name, input_target) {}

    private:
        bool paint_content() {
            const Rect rect = Rect::from_position_size(ImGui::GetCursorScreenPos(), layout().size());
            ImGui::Dummy(rect.size());
            paint(*ImGui::GetWindowDrawList(), rect, style());
            return true;
        };

        virtual void paint(ImDrawList& draw_list, Rect rect, const Style& style) = 0;
    };
} // namespace ui
