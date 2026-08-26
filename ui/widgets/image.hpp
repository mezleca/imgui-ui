#pragma once

#include "widget.hpp"

#include <imgui.h>

class IconTexture;

namespace ui {
    /// style padding insets the image while screen bounds retain the outer size.
    class ImageWidget : public Widget {
    public:
        explicit ImageWidget(IconTexture* texture = nullptr);

        ImageWidget& set_texture(IconTexture* texture) {
            m_texture = texture;
            return *this;
        }

        ImageWidget& set_rotation(float radians) {
            m_rotation = radians;
            return *this;
        }

        bool on_draw() override;

    private:
        IconTexture* m_texture = nullptr;
        float m_rotation = 0.0F;
    };

} // namespace ui
