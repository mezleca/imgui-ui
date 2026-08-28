#include "blur.hpp"

#include <algorithm>
#include <deque>

// https://github.com/itsRythem/ImGui-Blur

namespace ui {
    static std::deque<BlurRegion> regions;
    static ImDrawCallback callback = nullptr;

    void begin_blur_frame() {
        regions.clear();
    }

    void set_blur_callback(ImDrawCallback value) {
        callback = value;
    }

    void draw_blur(Rect rect, int strength, float rounding, float opacity) {
        if (callback == nullptr || strength <= 0 || opacity <= 0.0F || !rect.valid()) {
            return;
        }

        BlurRegion& region = regions.emplace_back(BlurRegion{rect, strength, rounding, std::clamp(opacity, 0.0F, 1.0F)});
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddCallback(callback, &region);
        draw_list->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
    }
} // namespace ui
