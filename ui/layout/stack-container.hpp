#pragma once

#include "child-container.hpp"

namespace ui {
    class StackContainer : public ChildContainer {
    public:
        explicit StackContainer(std::string id, StackDirection direction = StackDirection::Vertical);

        /// changes the main axis used to arrange visible children.
        void set_direction(StackDirection direction);
        StackDirection direction() const;
        /// sets the gap between visible children; negative values become zero.
        void set_spacing(float spacing);
        float spacing() const;
        /// sizes the container to its measured children instead of available space.
        StackContainer& fit_content(bool enabled = true);

    protected:
        bool on_draw() override;
        void on_measure() override;
        void on_layout() override;
        /// resolves each visible child's size and top-left placement on the main axis.
        void arrange_children(ImVec2 container_size);

    private:
        StackDirection m_direction;
        float m_spacing = 0.0F;
        ImVec2 m_content_size{};
        ImVec2 m_fit_size{};
        bool m_fits_content = false;
    };
} // namespace ui
