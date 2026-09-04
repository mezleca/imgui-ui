#pragma once

#include "widget.hpp"

#include <imgui.h>

namespace ui {
    class Texture;

    /// images are passive. call set_input_target() to route their pointer input.
    class ImageWidget : public DrawListWidget {
    public:
        explicit ImageWidget(Texture* texture = nullptr);

        ImageWidget& set_texture(Texture* texture) {
            m_texture = texture;
            return *this;
        }

        ImageWidget& set_rotation(float radians) {
            m_rotation = radians;
            return *this;
        }

    private:
        void paint_draw_list(ImDrawList& draw_list, Rect rect, const Style& style) override;
        Texture* m_texture = nullptr;
        float m_rotation = 0.0F;
    };

} // namespace ui
