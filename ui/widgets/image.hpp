#pragma once

#include "widget.hpp"

#include <cstdint>
#include <imgui.h>

namespace ui {
    class Texture;

    enum class ImageFit : uint8_t {
        Fill,
        Contain,
        Cover,
    };

    /// images are passive. call set_input_mode(InputMode::Target) to route their pointer input.
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
