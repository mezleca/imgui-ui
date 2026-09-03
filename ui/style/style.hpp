#pragma once

#include "theme.hpp"
#include "variables.hpp"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <type_traits>
#include <utility>
#include <variant>

namespace ui {
    class StyledNode;
    class VisualState;

    enum Border : uint8_t {
        BORDER_NONE = 0,
        BORDER_LEFT = 1 << 0,
        BORDER_TOP = 1 << 1,
        BORDER_RIGHT = 1 << 2,
        BORDER_BOTTOM = 1 << 3,
        BORDER_ALL = BORDER_LEFT | BORDER_TOP | BORDER_RIGHT | BORDER_BOTTOM,
    };

    enum class BorderStyle : uint8_t {
        Solid,
        Dashed,
        Dotted,
    };

    enum class StyleType : int32_t {
        DEFAULT = 0,
        HOVER = 1,
        ACTIVE,
        FOCUS,
        _COUNT
    };

    class Style {
    public:
        using ChangeCallback = void (*)(void*);

        struct PushState {
            bool font_pushed = false;
            int variables = 0;
            int colors = 0;
        };

        Style() : m_padding({}) {
            const Theme theme = Theme::defaults();
            m_color.set(theme.text_color);
            m_border_color.set(theme.border_color);
            m_background_color.set(theme.transparent);
        }

        ImFont* font() const {
            return m_font;
        }

        Style& font(ImFont* value) {
            if (m_font == value) {
                return *this;
            }

            m_font = value;
            notify_change();
            return *this;
        }

        const ImVec2& padding() const {
            return m_padding;
        }

        Style& padding(ImVec2 value) {
            const ImVec2 resolved = {std::max(0.0F, value.x), std::max(0.0F, value.y)};
            if (m_padding.x == resolved.x && m_padding.y == resolved.y) {
                return *this;
            }

            m_padding = resolved;
            notify_change();
            return *this;
        }

        Style& control(const Theme& theme, ImVec2 padding = {10.0F, 6.0F}) {
            return color(theme.text_color)
                .background_color(theme.control_background_color, 0.15F)
                .border_color(theme.control_border_color, 0.15F)
                .padding(padding)
                .border(BORDER_ALL)
                .border_radius(theme.control_rounding)
                .border_thickness(theme.control_border_thickness);
        }

        /// unitless multiplier applied to each text line's font height.
        float line_height() const {
            return m_line_height.value;
        }

        Style& line_height(float value, float transition_duration = -1.0F) {
            const float resolved = std::max(0.0F, value);
            const bool changed = m_line_height.value != resolved;
            if (!changed && transition_duration < 0.0F) {
                return *this;
            }

            if (changed) {
                m_line_height.set(resolved);
            }
            if (transition_duration >= 0.0F) m_line_height.set_duration(transition_duration);
            if (changed) {
                notify_change();
            }
            return *this;
        }

        float alpha() const {
            return m_alpha;
        }

        Style& alpha(float value) {
            m_alpha = std::clamp(value, 0.0F, 1.0F);
            return *this;
        }

        /// cursor used while the mouse hovers a node in this style.
        /// none restores the arrow cursor.
        ImGuiMouseCursor cursor() const {
            return m_cursor;
        }

        Style& cursor(ImGuiMouseCursor value) {
            m_cursor = value;
            return *this;
        }

        /// uses this style's background color behind the child scrollbar.
        bool use_background_for_scrollbar() const {
            return m_use_background_for_scrollbar;
        }

        Style& use_background_for_scrollbar(bool value = true) {
            m_use_background_for_scrollbar = value;
            return *this;
        }

        const ColorValue& color() const {
            return m_color;
        }

        ColorValue& color() {
            return m_color;
        }

        Style& color(ImColor value, float transition_duration = -1.0F) {
            m_color.set(value);
            if (transition_duration >= 0.0F) m_color.set_duration(transition_duration);
            return *this;
        }

        const ColorValue& border_color() const {
            return m_border_color;
        }

        ColorValue& border_color() {
            return m_border_color;
        }

        Style& border_color(ImColor value, float transition_duration = -1.0F) {
            m_border_color.set(value);
            if (transition_duration >= 0.0F) m_border_color.set_duration(transition_duration);
            return *this;
        }

        const ColorValue& background_color() const {
            return m_background_color;
        }

        ColorValue& background_color() {
            return m_background_color;
        }

        Style& background_color(ImColor value, float transition_duration = -1.0F) {
            m_background_color.set(value);
            if (transition_duration >= 0.0F) m_background_color.set_duration(transition_duration);
            return *this;
        }

        const BoxShadow& box_shadow() const {
            return m_box_shadow.value;
        }

        Style& box_shadow(BoxShadow value, float transition_duration = -1.0F) {
            value.blur = std::max(0.0F, value.blur);
            value.color.Value.w = std::clamp(value.color.Value.w, 0.0F, 1.0F);
            m_box_shadow.set(std::move(value));
            if (transition_duration >= 0.0F) m_box_shadow.set_duration(transition_duration);
            return *this;
        }

        int blur() const {
            return m_blur;
        }

        Style& blur(int value) {
            m_blur = std::max(0, value);
            return *this;
        }

        float border_radius() const {
            return m_border_radius;
        }

        Style& border_radius(float value) {
            m_border_radius = std::max(0.0F, value);
            return *this;
        }

        float border_thickness() const {
            return m_border_thickness;
        }

        Style& border_thickness(float value) {
            m_border_thickness = std::max(0.0F, value);
            return *this;
        }

        uint8_t border() const {
            return m_border;
        }

        Style& border(uint8_t value) {
            m_border = value & BORDER_ALL;
            return *this;
        }

        BorderStyle border_style() const {
            return m_border_style;
        }

        Style& border_style(BorderStyle value) {
            m_border_style = value;
            return *this;
        }

        StyleVariableStore& variables() {
            return m_vars;
        }

        const StyleVariableStore& variables() const {
            return m_vars;
        }

        static void lerp(Style& style, const Style& target, float dt) {
            const ImFont* previous_font = style.m_font;
            const ImVec2 previous_padding = style.m_padding;
            const float previous_line_height = style.m_line_height.value;

            style.m_font = target.m_font;
            style.m_padding = target.m_padding;
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

            if (previous_font != style.m_font || previous_padding.x != style.m_padding.x ||
                previous_padding.y != style.m_padding.y || previous_line_height != style.m_line_height.value) {
                style.notify_change();
            }
        }

        bool is_close_to(const Style& target, float epsilon) const {
            if (m_font != target.m_font || m_padding.x != target.m_padding.x || m_padding.y != target.m_padding.y ||
                !m_line_height.is_close(target.m_line_height, epsilon) || std::abs(m_alpha - target.m_alpha) > epsilon ||
                m_cursor != target.m_cursor || m_use_background_for_scrollbar != target.m_use_background_for_scrollbar ||
                m_border != target.m_border || m_blur != target.m_blur ||
                std::abs(m_border_thickness - target.m_border_thickness) > epsilon ||
                std::abs(m_border_radius - target.m_border_radius) > epsilon || m_border_style != target.m_border_style ||
                !m_box_shadow.is_close(target.m_box_shadow, epsilon)) {
                return false;
            }

            if (!m_color.is_close(target.m_color, epsilon) || !m_border_color.is_close(target.m_border_color, epsilon) ||
                !m_background_color.is_close(target.m_background_color, epsilon)) {
                return false;
            }

            return m_vars.for_each([&](const std::string& key, const StyleValue& value) {
                const StyleValue* target_value = target.m_vars.find(key);
                if (target_value == nullptr) {
                    return true;
                }

                return std::visit(
                    [&](const auto& current_value) {
                        using T = std::decay_t<decltype(current_value)>;
                        const T* typed_target = std::get_if<T>(target_value);
                        return typed_target == nullptr || current_value.is_close(*typed_target, epsilon);
                    },
                    value
                );
            });
        }

    private:
        friend class StyledNode;
        friend class VisualState;
        friend class PaintSlot;

        void set_change_callback(void* owner, ChangeCallback callback) {
            m_change_owner = owner;
            m_change_callback = callback;
        }

        void notify_change() const {
            if (m_change_callback != nullptr) {
                m_change_callback(m_change_owner);
            }
        }

        PushState push(float opacity, ImFont* effective_font) const;
        static void pop(PushState state);

        ImFont* m_font = nullptr;
        ImVec2 m_padding = {};
        FloatValue m_line_height{1.0F};
        float m_alpha = 1.0F;
        ImGuiMouseCursor m_cursor = ImGuiMouseCursor_None;
        bool m_use_background_for_scrollbar = true;
        int m_blur = 0;
        float m_border_thickness = 1.0F;
        float m_border_radius = 4.0F;
        BoxShadowValue m_box_shadow;
        ColorValue m_color;
        ColorValue m_border_color;
        ColorValue m_background_color;
        uint8_t m_border = BORDER_NONE;
        BorderStyle m_border_style = BorderStyle::Solid;
        StyleVariableStore m_vars;
        void* m_change_owner = nullptr;
        ChangeCallback m_change_callback = nullptr;
    };

} // namespace ui
