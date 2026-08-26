#include "image.hpp"

#include "../imgui/draw.hpp"
#include "../resources/icon.hpp"

#include <cmath>

namespace ui {
    ImageWidget::ImageWidget(IconTexture* texture) : Widget({}, "Image"), m_texture(texture) {}

    bool ImageWidget::on_draw() {
        const Style& style = this->style();
        const Rect outer = layout().screen_rect();
        const ImVec2 padding = style.padding();
        const Rect content = {
            {outer.min.x + padding.x, outer.min.y + padding.y},
            {outer.max.x - padding.x, outer.max.y - padding.y},
        };

        ImGui::Dummy(outer.size());

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(outer.min, outer.max, style.background_color().get_col(), style.border_radius());

        if (m_texture != nullptr && content.valid()) {
            const ImVec2 image_size = content.size();
            const ImTextureID texture_id = m_texture->get(image_size);
            if (m_rotation == 0.0F) {
                draw_list->AddImageRounded(
                    texture_id, content.min, content.max, {0, 0}, {1, 1}, style.color().get_col(), style.border_radius(),
                    ImDrawFlags_RoundCornersAll
                );
            } else {
                const ImVec2 center = {(content.min.x + content.max.x) * 0.5F, (content.min.y + content.max.y) * 0.5F};
                const ImVec2 half_size = {image_size.x * 0.5F, image_size.y * 0.5F};
                const float sine = std::sin(m_rotation);
                const float cosine = std::cos(m_rotation);
                const auto rotate = [&](ImVec2 point) {
                    return ImVec2{
                        center.x + point.x * cosine - point.y * sine,
                        center.y + point.x * sine + point.y * cosine,
                    };
                };

                draw_list->AddImageQuad(
                    texture_id, rotate({-half_size.x, -half_size.y}), rotate({half_size.x, -half_size.y}),
                    rotate({half_size.x, half_size.y}), rotate({-half_size.x, half_size.y}), {0, 0}, {1, 0}, {1, 1}, {0, 1},
                    style.color().get_col()
                );
            }
        }

        draw_border(outer, style);
        return true;
    }

} // namespace ui
