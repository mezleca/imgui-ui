#pragma once

#include "../widgets/widget.hpp"

#include <imgui.h>
#include <string>
#include <string_view>

namespace ui {
    class Container : public Widget {
    public:
        explicit Container(std::string id, std::string_view type_name = "Container", bool input_target = false);

        Container& set_scrollable(bool scrollable);

    protected:
        void on_layout() final;

        virtual void resolve_layout();
        virtual void arrange_children() {}

        bool paint() override;
        void on_draw_end() override;

        virtual bool accepts_imgui_input() const {
            return true;
        }

    private:
        bool m_scrollable = false;
    };
} // namespace ui
