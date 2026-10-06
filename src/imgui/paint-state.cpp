#include <imgui-ui/imgui/paint-state.hpp>

#include <bit>
#include <cassert>

using namespace ui;

ImVec4 PaintState::effect_clip(ImVec4 fallback) const {
    return m_effect_clips.empty() ? fallback : m_effect_clips.back();
}

void PaintState::push_effect_clip(ImVec4 clip) {
    m_effect_clips.push_back(clip);
}

void PaintState::pop_effect_clip() {
    assert(!m_effect_clips.empty());
    m_effect_clips.pop_back();
}

const BorderPath& PaintState::border_path(Rect rect, float rounding) {
    const uint32_t min_x = std::bit_cast<uint32_t>(rect.min.x);
    const uint32_t min_y = std::bit_cast<uint32_t>(rect.min.y);
    const uint32_t max_x = std::bit_cast<uint32_t>(rect.max.x);
    const uint32_t max_y = std::bit_cast<uint32_t>(rect.max.y);
    const uint32_t rounded = std::bit_cast<uint32_t>(rounding);
    const std::size_t index = (min_x ^ (min_y << 3U) ^ (max_x << 7U) ^ (max_y << 11U) ^ (rounded << 13U)) % m_border_paths.size();
    BorderEntry& entry = m_border_paths[index];

    if (entry.valid && entry.rect.min.x == rect.min.x && entry.rect.min.y == rect.min.y && entry.rect.max.x == rect.max.x &&
        entry.rect.max.y == rect.max.y && entry.rounding == rounding) {
        return entry.path;
    }

    entry.rect = rect;
    entry.rounding = rounding;
    entry.path = rounded_rect_border_path(rect, rounding);
    entry.valid = true;
    return entry.path;
}
