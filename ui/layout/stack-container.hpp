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
        StackDirection stack_direction() const override;
        float stack_spacing() const override;
        ImVec2 stack_content_alignment() const override;

    private:
        StackDirection m_direction;
        float m_spacing = 0.0F;
        ImVec2 m_content_alignment{};
    };
} // namespace ui
