#include "styled-node.hpp"

#include "decoration.hpp"

namespace ui {
    StyledNode::StyledNode(std::string id, std::string_view type_name) : Node(std::move(id)), m_type_name(type_name) {
        m_state.set_change_callback(this, &StyledNode::style_changed);
    }

    StyledNode::~StyledNode() = default;

    StyledNode& StyledNode::before() {
        if (m_before == nullptr) {
            m_before = std::make_unique<Decoration>("Before");
        }

        return *m_before;
    }

    StyledNode& StyledNode::after() {
        if (m_after == nullptr) {
            m_after = std::make_unique<Decoration>("After");
        }

        return *m_after;
    }

    void StyledNode::remove_before() {
        m_before.reset();
    }

    void StyledNode::remove_after() {
        m_after.reset();
    }

    void StyledNode::advance_frame_state(float dt) {
        m_state.update(dt);
        if (m_before != nullptr) m_before->update(dt);
        if (m_after != nullptr) m_after->update(dt);
    }

    void StyledNode::draw_before() {
        if (m_before != nullptr) m_before->draw_for(layout().screen_rect());
    }

    void StyledNode::draw_after() {
        if (m_after != nullptr) m_after->draw_for(layout().screen_rect());
    }
} // namespace ui
