#pragma once

#include "box-sizing.hpp"
#include "variables.hpp"

#include <cstdint>

namespace ui {
    class StyledNode;

    /// fractional layout coordinates can place a 1px dotted edge between pixel centers.
    inline constexpr float MIN_BORDER_THICKNESS = 1.5F;

    enum Border : uint8_t {
        BORDER_NONE = 0,
        BORDER_LEFT = 1U << 0U,
        BORDER_TOP = 1U << 1U,
        BORDER_RIGHT = 1U << 2U,
        BORDER_BOTTOM = 1U << 3U,

        BORDER_ALL = BORDER_LEFT | BORDER_TOP | BORDER_RIGHT | BORDER_BOTTOM,
    };

    enum class BorderStyle : uint8_t {
        Solid,
        Dashed,
        Dotted,
    };

    enum class Overflow : uint8_t {
        /// lets descendant effects escape. borderless containers also let normal surfaces escape.
        Visible,
        /// clips descendants and effects to the container while retaining enabled scrolling.
        Hidden,
        /// clips descendants and effects to the container and disables scrolling.
        Clip,
    };

    struct PushState {
        bool font_pushed = false;
        int variables = 0;
        int colors = 0;
    };

    class ComputedStyle {
    public:
        ComputedStyle();

        ImFont* font() const {
            return m_font;
        }

        const ImVec2& padding() const {
            return m_padding.value;
        }

        BoxSizing box_sizing() const {
            return m_box_sizing;
        }

        Overflow overflow() const {
            return m_overflow;
        }

        const ImVec2& margin() const {
            return m_margin.value;
        }

        float line_height() const {
            return m_line_height.value;
        }

        float rotation() const {
            return m_rotation.value;
        }

        const ImVec2& scale() const {
            return m_scale.value;
        }

        float alpha() const {
            return m_alpha;
        }

        ImGuiMouseCursor cursor() const {
            return m_cursor;
        }

        float scrollbar_size() const {
            return m_scrollbar_size;
        }

        float scrollbar_rounding() const {
            return m_scrollbar_rounding;
        }

        float scrollbar_minimum_grab_size() const {
            return m_scrollbar_minimum_grab_size;
        }

        float scrollbar_grab_rounding() const {
            return m_scrollbar_grab_rounding;
        }

        const ColorValue& scrollbar_background_color() const {
            return m_scrollbar_background_color;
        }

        const ColorValue& scrollbar_grab_color() const {
            return m_scrollbar_grab_color;
        }

        const ColorValue& scrollbar_grab_hovered_color() const {
            return m_scrollbar_grab_hovered_color;
        }

        const ColorValue& scrollbar_grab_active_color() const {
            return m_scrollbar_grab_active_color;
        }

        const ColorValue& color() const {
            return m_color;
        }

        const ColorValue& border_color() const {
            return m_border_color;
        }

        const ColorValue& background_color() const {
            return m_background_color;
        }

        const BoxShadow& box_shadow() const {
            return m_box_shadow.value;
        }

        int blur() const {
            return m_blur;
        }

        float border_radius() const {
            return m_border_radius;
        }

        float border_thickness() const {
            return m_border_thickness;
        }

        uint8_t border() const {
            return m_border;
        }

        BorderStyle border_style() const {
            return m_border_style;
        }

        const StyleVariableStore& variables() const {
            return m_vars;
        }

    protected:
        PushState push(float opacity, ImFont* effective_font) const;

        static void pop(PushState state);

        friend class StyledNode;

        ImFont* m_font = nullptr;
        Vec2Value m_margin;
        Vec2Value m_padding;
        FloatValue m_line_height{1.0F};
        FloatValue m_rotation{0.0F};
        Vec2Value m_scale{ImVec2{1.0F, 1.0F}};
        float m_alpha = 1.0F;
        ImGuiMouseCursor m_cursor = ImGuiMouseCursor_None;
        float m_scrollbar_size = 14.0F;
        float m_scrollbar_rounding = 9.0F;
        float m_scrollbar_minimum_grab_size = 12.0F;
        float m_scrollbar_grab_rounding = 9.0F;
        int m_blur = 0;
        float m_border_thickness = 1.0F;
        float m_border_radius = 4.0F;
        BoxSizing m_box_sizing = BoxSizing::ContentBox;
        Overflow m_overflow = Overflow::Visible;
        uint8_t m_border = BORDER_NONE;
        BorderStyle m_border_style = BorderStyle::Solid;
        BoxShadowValue m_box_shadow;
        ColorValue m_color;
        ColorValue m_border_color;
        ColorValue m_background_color;
        ColorValue m_scrollbar_background_color;
        ColorValue m_scrollbar_grab_color;
        ColorValue m_scrollbar_grab_hovered_color;
        ColorValue m_scrollbar_grab_active_color;
        StyleVariableStore m_vars;
    };
} // namespace ui
