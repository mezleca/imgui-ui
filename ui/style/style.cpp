#include "style.hpp"

#include <string>
#include <type_traits>
#include <variant>

using namespace ui;

// name, affects_measure
// affects_measure is true only when an interpolated value can change the measured geometry.
#define UI_STYLE_TRANSITION_PROPERTIES(X)                                                                                        \
    X(padding, true)                                                                                                             \
    X(line_height, true)                                                                                                         \
    X(box_shadow, false)                                                                                                         \
    X(color, false)                                                                                                              \
    X(border_color, false)                                                                                                       \
    X(background_color, false)

// expands the property list into one tick per animated property and records layout-affecting changes.
#define UI_STYLE_TICK_PROPERTY(name, affects_measure)                                                                            \
    if (style.m_##name.tick(target.m_##name, dt) && affects_measure) {                                                           \
        measure_changed = true;                                                                                                  \
    }

// expands the property list into early-return checks for properties that still have active transitions.
#define UI_STYLE_HAS_ACTIVE_TRANSITION(name, affects_measure)                                                                    \
    if (style.m_##name.is_transitioning()) return true;

bool Style::lerp(Style& style, const Style& target, float dt) {
    const ImFont* previous_font = style.m_font;
    bool measure_changed = false;

    style.m_font = target.m_font;
    style.m_alpha = target.m_alpha;
    style.m_cursor = target.m_cursor;
    style.m_use_background_for_scrollbar = target.m_use_background_for_scrollbar;
    style.m_blur = target.m_blur;
    style.m_border_thickness = target.m_border_thickness;
    style.m_border_radius = target.m_border_radius;
    style.m_border = target.m_border;
    style.m_border_style = target.m_border_style;

    UI_STYLE_TRANSITION_PROPERTIES(UI_STYLE_TICK_PROPERTY)

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

    if (previous_font != style.m_font || measure_changed) {
        style.notify_change();
    }

    UI_STYLE_TRANSITION_PROPERTIES(UI_STYLE_HAS_ACTIVE_TRANSITION)

    return style.m_vars.is_transitioning();
}

#undef UI_STYLE_TRANSITION_PROPERTIES
#undef UI_STYLE_TICK_PROPERTY
#undef UI_STYLE_HAS_ACTIVE_TRANSITION
