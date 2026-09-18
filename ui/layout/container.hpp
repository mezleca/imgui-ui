#pragma once

#include "../widgets/widget.hpp"

#include <string>
#include <string_view>

namespace ui {
    class Container : public Widget {
    public:
        explicit Container(std::string id, std::string_view type_name = "Container");
        Container(std::string id, StackDirection direction, std::string_view type_name = "Container");

        /// enables the requested scrollbars. imgui may still show a vertical scrollbar when horizontal scrolling is enabled,
        /// even if vertical is false.
        Container& set_scrollable(bool vertical, bool horizontal = false);
        /// orders visible in-flow children along one axis.
        Container& set_direction(StackDirection direction);
        /// offsets the complete flow group within the available content box.
        Container& set_content_alignment(Anchor alignment);
        Container& set_content_alignment(ImVec2 alignment);
        /// inserts a gap between adjacent visible in-flow children.
        Container& set_spacing(float spacing);

    protected:
        void on_layout() final;
        void apply_theme_defaults(const Theme& theme) override;

        virtual void resolve_layout();
        void on_measure() override;
        virtual void arrange_children();
        void arrange_children(ImVec2 content_size);
        void draw_children() override;

        virtual ImVec2 child_window_padding() const;
        virtual ImVec2 child_window_size() const;
        /// returns flags for the child window opened by paint. subclasses use it to change native hit testing.
        virtual ImGuiWindowFlags child_window_flags() const;
        virtual Rect shadow_rect(Rect child_rect) const;
        bool paint() override;
        void on_draw_end() override;

        virtual ImVec2 child_layout_size() const;
        virtual ImVec2 child_window_content_size() const;

    private:
        bool m_scroll_vertical = false;
        bool m_scroll_horizontal = false;
        StackDirection m_direction = StackDirection::Vertical;
        float m_spacing = 0.0F;
        ImVec2 m_content_alignment{};
        ImVec2 m_content_size{};
        bool m_content_clip_pushed = false;
    };
} // namespace ui
