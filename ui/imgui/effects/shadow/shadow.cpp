#include "shadow.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <unordered_map>

struct BoxShadowPass {
    std::deque<ui::BoxShadowRegion> regions;
    ImDrawCallback callback = nullptr;
};

static std::unordered_map<ImGuiContext*, BoxShadowPass> passes;
static BoxShadowPass* current_pass = nullptr;

static float shadow_rounding(float rounding, float spread) {
    rounding = std::max(0.0F, rounding);
    if (spread > 0.0F && rounding < spread) {
        const float ratio = rounding / spread;
        spread *= 1.0F + (ratio - 1.0F) * (ratio - 1.0F) * (ratio - 1.0F);
    }

    return std::max(0.0F, rounding + spread);
}

void ui::begin_box_shadow_frame() {
    current_pass = nullptr;
    if (ImGuiContext* context = ImGui::GetCurrentContext(); context != nullptr) {
        current_pass = &passes[context];
        current_pass->regions.clear();
    }
}

void ui::set_box_shadow_callback(ImDrawCallback callback) {
    if (ImGuiContext* context = ImGui::GetCurrentContext(); context != nullptr) {
        current_pass = &passes[context];
        current_pass->callback = callback;
    }
}

void ui::shutdown_box_shadow() {
    if (ImGuiContext* context = ImGui::GetCurrentContext(); context != nullptr) {
        passes.erase(context);
    }
    current_pass = nullptr;
}

void ui::draw_box_shadow(ImDrawList& draw_list, Rect rect, const BoxShadow& shadow, float rounding, float opacity) {
    if (current_pass == nullptr || current_pass->callback == nullptr || opacity <= 0.0F || shadow.color.Value.w <= 0.0F ||
        !rect.valid()) {
        return;
    }

    const float spread = shadow.spread;
    const Rect shape = {
        {rect.min.x + shadow.offset.x - spread, rect.min.y + shadow.offset.y - spread},
        {rect.max.x + shadow.offset.x + spread, rect.max.y + shadow.offset.y + spread},
    };
    if (!shape.valid()) {
        return;
    }

    const float blur = std::max(0.0F, shadow.blur);
    const float extent = std::max(1.0F, blur * 1.5F);
    const Rect bounds = {{shape.min.x - extent, shape.min.y - extent}, {shape.max.x + extent, shape.max.y + extent}};
    BoxShadowRegion& region = current_pass->regions.emplace_back();
    region.shape = shape;
    region.bounds = bounds;
    region.rounding = shadow_rounding(rounding, spread);
    region.blur = blur;
    region.color = shadow.color.Value;
    region.color.w *= std::clamp(opacity, 0.0F, 1.0F);

    draw_list.AddCallback(current_pass->callback, &region);
    draw_list.AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}
