#pragma once

#include "../layout/geometry.hpp"
#include "../style/computed-style.hpp"

#include <optional>
#include <string_view>

namespace ui {
    class EffectRegistry;
    class GenericValue;
    class PaintState;
    enum class TriangleDirection : uint8_t {
        Up,
        Down,
        Left,
        Right,
    };

    class Painter {
    public:
        /// borrows the draw list, state and effects for this paint scope. recreate inside child or popup windows.
        /// captures imgui style alpha times opacity. drawing methods apply it to the supplied colors once.
        explicit Painter(
            ImDrawList& draw_list, PaintState* state = nullptr, EffectRegistry* effects = nullptr, float opacity = 1.0F
        );

        /// direct imgui calls bypass painter alpha. apply alpha() to their colors explicitly.
        ImDrawList& draw_list() const {
            return m_draw_list;
        }

        float alpha() const {
            return m_alpha;
        }

        void line(ImVec2 start, ImVec2 end, const Color& source, float thickness) const;
        void circle(ImVec2 center, float radius, const Color& source) const;
        void circle_outline(ImVec2 center, float radius, const Color& source, float thickness) const;
        void
        rect_filled(Rect rect, const Color& source, float rounding = 0.0F, ImDrawFlags flags = ImDrawFlags_RoundCornersAll) const;
        void rect_outline(Rect rect, const Color& source, float thickness = 1.0F, float rounding = 0.0F) const;
        void triangle(ImVec2 center, ImVec2 size, const Color& source, TriangleDirection direction) const;
        void image(ImTextureID texture, Rect rect, ImVec2 uv_min, ImVec2 uv_max, const Color& tint, float rounding = 0.0F) const;

        void text(ImVec2 position, const Color& source, std::string_view text) const;
        void text(ImVec2 position, const Color& source, const GenericValue& text, const ImVec4* clip_rect = nullptr) const;
        void text_ellipsis(ImVec2 position, const Color& source, const GenericValue& text, ImVec4 clip_rect) const;

        void frame(Rect rect, const ComputedStyle& style, const std::optional<Color>& background = {}) const;
        /// paints background and border after a container has emitted effects with its ancestor clip.
        void frame_surface(Rect rect, const ComputedStyle& style, const std::optional<Color>& background = {}) const;
        void blur(Rect rect, const ComputedStyle& style) const;
        void shadow(Rect rect, const ComputedStyle& style) const;
        void border(Rect rect, const ComputedStyle& style, const Color& source) const;

    private:
        ImColor color(const Color& source) const;
        void frame_effects(Rect rect, const ComputedStyle& style) const;

        ImDrawList& m_draw_list;
        PaintState* m_state;
        EffectRegistry* m_effects;
        float m_alpha;
    };
} // namespace ui
