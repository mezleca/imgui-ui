#pragma once

#include "../widgets/widget.hpp"

#include <string>
#include <string_view>

namespace ui {
    class Container : public Widget {
    public:
        explicit Container(std::string id, std::string_view type_name = "Container");

        /// enables the requested scrollbars. imgui may still show a vertical scrollbar when horizontal scrolling is enabled,
        /// even if vertical is false.
        Container& set_scrollable(bool vertical, bool horizontal = false);

    protected:
        void on_layout() final;

        virtual void resolve_layout();
        virtual void arrange_children() {}
        void draw_children() override;

        bool paint() override;
        void on_draw_end() override;

    private:
        bool m_scroll_vertical = false;
        bool m_scroll_horizontal = false;
    };
} // namespace ui
