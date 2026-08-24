#include "image.hpp"

#include "../imgui/draw.hpp"
#include "../ui.hpp"
#include "../resources/icon.hpp"

namespace ui {
    ImageWidget::ImageWidget(IconTexture* texture) : Widget({}, "Image"), m_texture(texture) {}

    bool ImageWidget::on_draw() {
        const Style& style = this->style();
        const ImVec2 outer_size = layout().size();
        const ImVec2 outer_min = ImGui::GetCursorScreenPos();
        const Rect outer = Rect::from_position_size(outer_min, outer_size);
        const ImVec2 padding = style.padding();
        const Rect content = {
            {outer.min.x + padding.x, outer.min.y + padding.y},
            {outer.max.x - padding.x, outer.max.y - padding.y},
        };

        ImGui::Dummy(outer_size);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(outer.min, outer.max, style.background_color().get_col(), style.border_radius());

        if (m_texture != nullptr && content.valid()) {
            const ImVec2 image_size = content.size();
            const ImTextureID texture_id = m_texture->get(image_size);
            draw_list->AddImageRounded(
                texture_id, content.min, content.max, {0, 0}, {1, 1}, style.color().get_col(), style.border_radius(),
                ImDrawFlags_RoundCornersAll
            );
        }

        draw_border(outer, style);
        return true;
    }

} // namespace ui
