#pragma once

#include "styled-node.hpp"

namespace ui {
    class Decoration final : public StyledNode {
    public:
        explicit Decoration(std::string_view type_name) : StyledNode({}, type_name) {}

    private:
        friend class StyledNode;

        void draw_for(Rect rect);
    };
} // namespace ui
