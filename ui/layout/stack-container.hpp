#pragma once

#include "child-container.hpp"

namespace ui {
    class StackContainer : public ChildContainer {
    public:
        explicit StackContainer(std::string id, StackDirection direction = StackDirection::Vertical);

        /// changes the main axis used to arrange visible children.
        StackContainer& set_direction(StackDirection direction);
        StackDirection direction() const;
        /// sets the gap between visible children; negative values become zero.
        StackContainer& set_spacing(float spacing);
        float spacing() const;
        /// sizes both axes to their measured children instead of available space.
        StackContainer& fit_content(bool enabled = true);
        /// sizes only the horizontal axis to measured children.
        StackContainer& fit_content_width(bool enabled = true);
        /// sizes only the vertical axis to measured children.
        StackContainer& fit_content_height(bool enabled = true);

    protected:
        bool paint_content() override;
        void on_measure() override;
        void on_layout() override;
        ImVec2 requested_size_for_layout() const override;
        /// resolves each visible child's size and top-left placement on the main axis.
        void arrange_children(ImVec2 container_size);

    private:
        StackDirection m_direction;
        float m_spacing = 0.0F;
        ImVec2 m_content_size{};
        ImVec2 m_fit_size{};
        bool m_fit_width = false;
        bool m_fit_height = false;
    };
} // namespace ui
