#pragma once

#include "container.hpp"

namespace ui {
    class StackContainer : public Container {
    public:
        explicit StackContainer(std::string id, StackDirection direction = StackDirection::Vertical);

        StackContainer& set_direction(StackDirection direction);
        StackContainer& set_content_alignment(Anchor alignment);
        StackContainer& set_content_alignment(ImVec2 alignment);
        StackContainer& set_spacing(float spacing);

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
