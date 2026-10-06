#pragma once

#include "border.hpp"

#include <array>
#include <vector>

namespace ui {
    class PaintState {
    public:
        ImVec4 effect_clip(ImVec4 fallback) const;
        void push_effect_clip(ImVec4 clip);
        void pop_effect_clip();

        const BorderPath& border_path(Rect rect, float rounding);

    private:
        struct BorderEntry {
            Rect rect;
            float rounding = 0.0F;
            BorderPath path;
            bool valid = false;
        };

        std::vector<ImVec4> m_effect_clips;
        std::array<BorderEntry, 64> m_border_paths{};
    };
} // namespace ui
