#pragma once

#include "widget.hpp"

#include <cstdint>
#include <imgui.h>

namespace ui {
    class Texture;

    enum class ImageFit : uint8_t {
        /// stretches the texture to fill the widget rectangle.
        Fill,
        /// fits the complete texture inside the widget rectangle.
        Contain,
        /// fills the widget rectangle and crops overflow.
        Cover,
    };

    /// images are passive until input mode target is enabled.
    class ImageWidget : public DrawListWidget {
    public:
        explicit ImageWidget(Texture* texture = nullptr);

        ImageWidget& set_texture(Texture* texture) {
            m_texture = texture;
            return *this;
        }

        ImageWidget& set_fit(ImageFit fit) {
            m_fit = fit;
            return *this;
        }

        ImageFit fit() const {
            return m_fit;
        }

    private:
        void paint_draw_list(ImDrawList& draw_list, Rect rect, const ComputedStyle& style) override;
        Texture* m_texture = nullptr;
        ImageFit m_fit = ImageFit::Fill;
    };

} // namespace ui
