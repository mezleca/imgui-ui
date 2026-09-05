#pragma once

#include "theme.hpp"
#include "variables.hpp"

#include <cstdint>

namespace ui {
    class Style;

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

    class ComputedStyle {
    public:
        struct PushState {
            bool font_pushed = false;
            int variables = 0;
            int colors = 0;
        };

        ComputedStyle();

        ImFont* font() const {
            return m_font;
        }

        const ImVec2& padding() const {
            return m_padding.value;
        }

        /// returns the unitless multiplier applied to each measured text line.
        float line_height() const {
            return m_line_height.value;
        }

        float alpha() const {
            return m_alpha;
        }

        /// returns the cursor used while the mouse hovers a node with this style.
        ImGuiMouseCursor cursor() const {
            return m_cursor;
        }

        /// returns whether the style background is drawn behind a child scrollbar.
        bool use_background_for_scrollbar() const {
            return m_use_background_for_scrollbar;
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

        /// pushes resolved style values into imgui and records exactly what must be restored.
        PushState push(float opacity, ImFont* effective_font) const;

        /// restores the imgui values recorded by push.
        static void pop(PushState state);

        /// reports whether all animated and static values reached the target within epsilon.
        bool is_close_to(const ComputedStyle& target, float epsilon) const;

    protected:
        friend class Style;

        ImFont* m_font = nullptr;
        Vec2Value m_padding;
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
    };
} // namespace ui
