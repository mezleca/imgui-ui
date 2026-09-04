#pragma once

#include "asset.hpp"

#include <imgui.h>
#include <cstdint>
#include <filesystem>
#include <unordered_map>

namespace ui {
    enum FontVariant : int32_t {
        FONT_EXTRA_SMALL = 10,
        FONT_SMALL = 14,
        FONT_MEDIUM = 20,
        FONT_LARGE = 26,
        FONT_EXTRA_LARGE = 32
    };

    struct ContextFonts {
        ImGuiIO* io = nullptr;
        std::unordered_map<int, ImFont*> fonts;
    };

    class Font final : public Asset {
    public:
        void initialize(ImFontConfig cfg, std::filesystem::path location);
        ImFont* get(int size);
        bool load(int size);
        void release_context(ImGuiContext* context) override;

    private:
        ImFont* load_font_variation(ImGuiContext* context, int size);

        std::filesystem::path m_font_location;
        std::unordered_map<ImGuiContext*, ContextFonts> m_contexts;
        ImFontConfig m_cfg;
    };

} // namespace ui
