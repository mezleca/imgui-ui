#pragma once

#include "container.hpp"

namespace ui {
    class StackContainer : public Container {
    public:
        explicit StackContainer(std::string id, StackDirection direction = StackDirection::Vertical);

        /// changes the main axis used to arrange visible children.
        StackContainer& set_direction(StackDirection direction);
        StackDirection direction() const;
        StackContainer& set_content_alignment(Anchor alignment);
        StackContainer& set_content_alignment(ImVec2 alignment);
        /// sets the gap between visible children. negative values become zero.
        StackContainer& set_spacing(float spacing);
        float spacing() const;

    protected:
        bool paint() override;
        void on_measure() override;
        void arrange_children() override;

    private:
        ImVec2 resolve_child_size(const Node& child, ImVec2 content_size, float flexible_main) const;

        StackDirection m_direction;
        float m_spacing = 0.0F;
        ImVec2 m_content_size{};
        ImVec2 m_content_alignment{};
    };
} // namespace ui
