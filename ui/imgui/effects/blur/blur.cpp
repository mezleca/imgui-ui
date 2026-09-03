#include "blur.hpp"

#include <algorithm>
#include <deque>
#include <unordered_map>

using namespace ui;

struct BlurPass {
    std::deque<BlurRegion> regions;
    ImDrawCallback callback = nullptr;
};

static std::unordered_map<ImGuiContext*, BlurPass> passes;
static BlurPass* current_pass = nullptr;

void ui::begin_blur_frame() {
    current_pass = nullptr;
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context != nullptr) {
        current_pass = &passes[context];
        current_pass->regions.clear();
    }
}

void ui::set_blur_callback(ImDrawCallback value) {
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context != nullptr) {
        current_pass = &passes[context];
        current_pass->callback = value;
    }
}

void ui::shutdown_blur() {
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context != nullptr) {
        passes.erase(context);
    }
    current_pass = nullptr;
}

void ui::draw_blur(ImDrawList& draw_list, Rect rect, int strength, float rounding, float opacity) {
    if (current_pass == nullptr || current_pass->callback == nullptr || strength <= 0 || opacity <= 0.0F || !rect.valid()) {
        return;
    }

    BlurRegion& region =
        current_pass->regions.emplace_back(BlurRegion{rect, strength, rounding, std::clamp(opacity, 0.0F, 1.0F)});
    draw_list.AddCallback(current_pass->callback, &region);
    draw_list.AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}
