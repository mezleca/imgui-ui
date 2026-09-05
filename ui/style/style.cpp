#include "style.hpp"

#include <string>
#include <type_traits>
#include <variant>

namespace ui {
    void Style::lerp(Style& style, const Style& target, float dt) {
        const ImFont* previous_font = style.m_font;
        const ImVec2 previous_padding = style.m_padding.value;
        const float previous_line_height = style.m_line_height.value;

        style.m_font = target.m_font;
        style.m_padding.tick(target.m_padding, dt);
        style.m_alpha = target.m_alpha;
        style.m_cursor = target.m_cursor;
        style.m_use_background_for_scrollbar = target.m_use_background_for_scrollbar;
        style.m_blur = target.m_blur;
        style.m_border_thickness = target.m_border_thickness;
        style.m_border_radius = target.m_border_radius;
        style.m_border = target.m_border;
        style.m_border_style = target.m_border_style;
        style.m_box_shadow.tick(target.m_box_shadow, dt);
        style.m_color.tick(target.m_color, dt);
        style.m_border_color.tick(target.m_border_color, dt);
        style.m_background_color.tick(target.m_background_color, dt);
        style.m_line_height.tick(target.m_line_height, dt);

        style.m_vars.for_each([&](const std::string& key, StyleValue& value) {
            const StyleValue* target_value = target.m_vars.find(key);

            if (target_value == nullptr) {
                return true;
            }

            std::visit(
                [&](auto& current_value) {
                    using T = std::decay_t<decltype(current_value)>;
                    if (const T* typed_target = std::get_if<T>(target_value)) {
                        current_value.tick(*typed_target, dt);
                    }
                },
                value
            );

            return true;
        });

        if (previous_font != style.m_font || previous_padding.x != style.m_padding.value.x ||
            previous_padding.y != style.m_padding.value.y || previous_line_height != style.m_line_height.value) {
            style.notify_change();
        }
    }
} // namespace ui
