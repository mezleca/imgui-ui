#include "image.hpp"
#include "../imgui/draw.hpp"
#include "../resources/texture-registry.hpp"

#include <algorithm>

using namespace ui;

ImageWidget::ImageWidget(Texture* texture) : DrawListWidget({}, "Image", InputMode::None), m_texture(texture) {}

void ImageWidget::paint_draw_list(ImDrawList& draw_list, Rect rect, const ComputedStyle& style) {
    const Rect content = rect.inset(style.padding());
    draw_frame(draw_list, rect, style);

    if (m_texture != nullptr && content.valid()) {
        const ImVec2 content_size = content.size();
        Rect image = content;

        if (m_fit != ImageFit::Fill) {
            const ImVec2 texture_size = m_texture->size();
            if (texture_size.x > 0.0F && texture_size.y > 0.0F) {
                const float scale = m_fit == ImageFit::Contain
                                        ? std::min(content_size.x / texture_size.x, content_size.y / texture_size.y)
                                        : std::max(content_size.x / texture_size.x, content_size.y / texture_size.y);
                const ImVec2 size = {texture_size.x * scale, texture_size.y * scale};
                image = Rect::from_position_size(
                    {content.min.x + (content_size.x - size.x) * 0.5F, content.min.y + (content_size.y - size.y) * 0.5F}, size
                );
            }
        }

        const ImTextureID texture_id = m_texture->get(image.size());
        const bool clip_image = m_fit == ImageFit::Cover;
        if (clip_image) {
            draw_list.PushClipRect(content.min, content.max, true);
        }

        draw_list.AddImageRounded(
            texture_id, image.min, image.max, {0, 0}, {1, 1}, style.color().get_col(), style.border_radius(),
            ImDrawFlags_RoundCornersAll
        );

        if (clip_image) {
            draw_list.PopClipRect();
        }
    }
}
