#pragma once

#include "asset-registry.hpp"

#include <filesystem>
#include <imgui.h>
#include <misc/freetype/imgui_freetype.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace ui {
    class Font final {
    public:
        Font(std::filesystem::path location, ImFontConfig config);

        // each imgui context owns its atlas fonts. cache pointers by context and requested size.
        ImFont* get(int size);
        void release_context(ImGuiContext* context);

    private:
        std::filesystem::path m_location;
        std::unordered_map<ImGuiContext*, std::unordered_map<int, ImFont*>> m_contexts;
        ImFontConfig m_config;
    };

    class FontRegistry final : public AssetRegistry<Font> {
    public:
        Font* add(std::string id, std::filesystem::path location);
        Font* add(std::string id, std::filesystem::path location, ImFontConfig config);
    };
} // namespace ui
