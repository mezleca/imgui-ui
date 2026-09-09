#pragma once

#include "variables.hpp"

#include <cstdint>

namespace ui {
    class Style;
    class StyledNode;

    /// fractional layout coordinates can place a 1px dotted edge between pixel centers.
    inline constexpr float MIN_BORDER_THICKNESS = 1.5F;

    enum Border : uint8_t {
        BORDER_NONE = 0,
        BORDER_LEFT = 1 << 0,
        BORDER_TOP = 1 << 1,
        BORDER_RIGHT = 1 << 2,
        BORDER_BOTTOM = 1 << 3,
        BORDER_ALL = BORDER_LEFT | BORDER_TOP | BORDER_RIGHT | BORDER_BOTTOM,
    };

    enum class BorderStyle : uint8_t {
        /// draws one continuous stroke per selected side.
        Solid,
        /// draws repeated rectangular dash segments.
        Dashed,
        /// draws repeated round dot segments.
        Dotted,
    };

    /// records the imgui state entries pushed by one style pass.
    struct PushState {
        /// records whether push changed the active font.
        bool font_pushed = false;
        /// counts style variables pushed by push.
        int variables = 0;
        /// counts style colors pushed by push.
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

    protected:
        friend class Style;

        /// pushes resolved style values into imgui and records exactly what must be restored.
        PushState push(float opacity, ImFont* effective_font) const;

        /// restores the imgui values recorded by push.
        void pop(PushState state) const;

        friend class StyledNode;

        ImFont* m_font = nullptr;
        Vec2Value m_margin;
        Vec2Value m_padding;
        FloatValue m_line_height{1.0F};
        FloatValue m_rotation{0.0F};
        Vec2Value m_scale{ImVec2{1.0F, 1.0F}};
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
