#pragma once

#include "widget.hpp"

#include <string>
#include <utility>

namespace ui {
    class BoxWidget : public DrawListWidget {
    public:
        explicit BoxWidget(std::string id = {}, LayoutSize size = {fit(), fit()})
            : DrawListWidget(std::move(id), "Box", InputMode::None) {
            set_size(size);
        }

    private:
        void paint_draw_list(ImDrawList& draw_list, Rect rect, const ComputedStyle&) override {
            draw_surface(draw_list, rect, computed_style());
        }
    };
} // namespace ui
