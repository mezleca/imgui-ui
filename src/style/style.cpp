#include <imgui-ui/style/style.hpp>

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <variant>

using namespace ui;

static ImVec2 nonnegative_insets(ImVec2 value) {
    return {std::max(0.0F, value.x), std::max(0.0F, value.y)};
}

Style& Style::font(ImFont* value) {
    if (m_font == value) return *this;

    m_font = value;
    notify_change(true);
    return *this;
}

Style& Style::padding(ImVec2 value, TransitionSpec transition) {
    return set_animated_transition(&Style::m_padding, nonnegative_insets(value), transition);
}

Style& Style::margin(ImVec2 value, TransitionSpec transition) {
    return set_animated_transition(&Style::m_margin, nonnegative_insets(value), transition);
}

Style& Style::line_height(float value, TransitionSpec transition) {
    return set_animated_transition(&Style::m_line_height, std::max(0.0F, value), transition);
}

Style& Style::box_sizing(BoxSizing value) {
    return set_property(&Style::m_box_sizing, value);
}

Style& Style::overflow(Overflow value) {
    return set_property(&Style::m_overflow, value);
}

Style& Style::rotation(float value, TransitionSpec transition) {
    return set_animated_transition(&Style::m_rotation, std::isfinite(value) ? value : 0.0F, transition, false);
}

Style& Style::scale(ImVec2 value, TransitionSpec transition) {
    value = {std::isfinite(value.x) ? value.x : 1.0F, std::isfinite(value.y) ? value.y : 1.0F};
    return set_animated_transition(&Style::m_scale, value, transition, false);
}

Style& Style::scale(float value, TransitionSpec transition) {
    return scale({value, value}, transition);
}

Style& Style::alpha(float value) {
    return set_property(&Style::m_alpha, std::clamp(value, 0.0F, 1.0F), false);
}

Style& Style::cursor(ImGuiMouseCursor value) {
    return set_property(&Style::m_cursor, value, false);
}

Style& Style::control(const Theme& theme, ImVec2 padding, TransitionSpec transition) {
    return color(theme.text_color)
        .background_color(theme.controls.background_color, transition)
        .border_color(theme.controls.border_color, transition)
        .padding(padding)
        .border(BORDER_ALL)
        .border_radius(theme.controls.rounding)
        .border_thickness(theme.controls.border_thickness);
}

Style& Style::color(Color value, TransitionSpec transition) {
    return set_animated_transition(&Style::m_color, std::move(value), transition, false);
}

Style& Style::background_color(Color value, TransitionSpec transition) {
    return set_animated_transition(&Style::m_background_color, std::move(value), transition, false);
}

Style& Style::border_color(Color value, TransitionSpec transition) {
    return set_animated_transition(&Style::m_border_color, std::move(value), transition, false);
}

Style& Style::border_radius(float value) {
    return set_property(&Style::m_border_radius, std::max(0.0F, value));
}

Style& Style::border_thickness(float value) {
    return set_property(&Style::m_border_thickness, value <= 0.0F ? 0.0F : std::max(MIN_BORDER_THICKNESS, value));
}

Style& Style::border(uint8_t value) {
    return set_property(&Style::m_border, static_cast<uint8_t>(value & BORDER_ALL));
}

Style& Style::border_style(BorderStyle value) {
    return set_property(&Style::m_border_style, value);
}

Style& Style::box_shadow(BoxShadow value, TransitionSpec transition) {
    value.blur = std::max(0.0F, value.blur);

    if (value.color.gradient() == nullptr) {
        ImVec4 solid = value.color.rgba();
        solid.w = std::clamp(solid.w, 0.0F, 1.0F);
        value.color = solid;
    }

    return set_animated_transition(&Style::m_box_shadow, std::move(value), transition);
}

Style& Style::blur(int value) {
    return set_property(&Style::m_blur, std::clamp(value, 0, MAX_BLUR_STRENGTH));
}

Style& Style::scrollbar(const Theme::Scrollbar& value) {
    return scrollbar_size(value.size)
        .scrollbar_rounding(value.rounding)
        .scrollbar_minimum_grab_size(value.minimum_grab_size)
        .scrollbar_grab_rounding(value.grab_rounding)
        .scrollbar_background_color(value.background_color)
        .scrollbar_grab_color(value.grab_color)
        .scrollbar_grab_hovered_color(value.grab_hovered_color)
        .scrollbar_grab_active_color(value.grab_active_color);
}

Style& Style::scrollbar_size(float value) {
    return set_property(&Style::m_scrollbar_size, std::max(0.0F, value));
}

Style& Style::scrollbar_rounding(float value) {
    return set_property(&Style::m_scrollbar_rounding, std::max(0.0F, value));
}

Style& Style::scrollbar_minimum_grab_size(float value) {
    return set_property(&Style::m_scrollbar_minimum_grab_size, std::max(1.0F, value));
}

Style& Style::scrollbar_grab_rounding(float value) {
    return set_property(&Style::m_scrollbar_grab_rounding, std::max(0.0F, value));
}

Style& Style::scrollbar_background_color(Color value, TransitionSpec transition) {
    return set_animated_transition(&Style::m_scrollbar_background_color, std::move(value), transition);
}

Style& Style::scrollbar_grab_color(Color value, TransitionSpec transition) {
    return set_animated_transition(&Style::m_scrollbar_grab_color, std::move(value), transition);
}

Style& Style::scrollbar_grab_hovered_color(Color value, TransitionSpec transition) {
    return set_animated_transition(&Style::m_scrollbar_grab_hovered_color, std::move(value), transition);
}

Style& Style::scrollbar_grab_active_color(Color value, TransitionSpec transition) {
    return set_animated_transition(&Style::m_scrollbar_grab_active_color, std::move(value), transition);
}

bool Style::lerp(Style& style, const Style& target, float dt) {
    const ImFont* previous_font = style.m_font;
    const BoxSizing previous_box_sizing = style.m_box_sizing;
    bool measure_changed = false;

    style.m_font = target.m_font;
    style.m_box_sizing = target.m_box_sizing;
    style.m_overflow = target.m_overflow;
    style.m_alpha = target.m_alpha;
    style.m_cursor = target.m_cursor;
    style.m_scrollbar_size = target.m_scrollbar_size;
    style.m_scrollbar_rounding = target.m_scrollbar_rounding;
    style.m_scrollbar_minimum_grab_size = target.m_scrollbar_minimum_grab_size;
    style.m_scrollbar_grab_rounding = target.m_scrollbar_grab_rounding;
    style.m_blur = target.m_blur;
    style.m_border_thickness = target.m_border_thickness;
    style.m_border_radius = target.m_border_radius;
    style.m_border = target.m_border;
    style.m_border_style = target.m_border_style;

    bool transitioning = false;
    const auto tick = [&](auto& current, const auto& destination, bool affects_measure = false) {
        const bool changed = current.tick(destination, dt);
        measure_changed |= changed && affects_measure;
        transitioning |= current.is_transitioning();
    };

    // record changes to margin, padding and line height. notify the owner after all properties have advanced.
    tick(style.m_margin, target.m_margin, true);
    tick(style.m_padding, target.m_padding, true);
    tick(style.m_line_height, target.m_line_height, true);

    tick(style.m_rotation, target.m_rotation);
    tick(style.m_scale, target.m_scale);

    tick(style.m_box_shadow, target.m_box_shadow);
    tick(style.m_color, target.m_color);
    tick(style.m_border_color, target.m_border_color);
    tick(style.m_background_color, target.m_background_color);

    tick(style.m_scrollbar_background_color, target.m_scrollbar_background_color);
    tick(style.m_scrollbar_grab_color, target.m_scrollbar_grab_color);
    tick(style.m_scrollbar_grab_hovered_color, target.m_scrollbar_grab_hovered_color);
    tick(style.m_scrollbar_grab_active_color, target.m_scrollbar_grab_active_color);

    for (auto& [key, value] : style.m_vars) {
        const StyleValue* target_value = target.m_vars.find(key);

        std::visit(
            [&](auto& current_value) {
                using T = std::decay_t<decltype(current_value)>;
                const T* typed_target = target_value == nullptr ? nullptr : std::get_if<T>(target_value);
                if (typed_target != nullptr) {
                    current_value.tick(*typed_target, dt);
                }
                transitioning |= current_value.is_transitioning();
            },
            value
        );
    }

    if (previous_font != style.m_font || previous_box_sizing != style.m_box_sizing || measure_changed) {
        style.notify_change(previous_font != style.m_font);
    }

    return transitioning;
}
