#pragma once

#include "../widgets/widget.hpp"

#include <string>
#include <string_view>

namespace ui {
    /// returns the computed margin, or zero for unstyled nodes.
    ImVec2 layout_margin(const Node& node);

    class Container : public Widget {
    public:
        explicit Container(std::string id, std::string_view type_name = "Container");

        Container& set_scrollable(bool scrollable);

    protected:
        void on_layout() final;

        virtual void resolve_layout();
        virtual void arrange_children() {}
        void draw_children() override;

        bool paint() override;
        void on_draw_end() override;

    private:
        bool m_scrollable = false;
    };
} // namespace ui
