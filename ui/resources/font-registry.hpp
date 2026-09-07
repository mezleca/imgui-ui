#pragma once

#include "asset-registry.hpp"

#include <filesystem>
#include <imgui.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace ui {
    struct ContextFonts {
        std::unordered_map<int, ImFont*> fonts;
    };

    class Font final {
    public:
        explicit Font(std::filesystem::path location, ImFontConfig cfg = {});

        ImFont* get(int size);
        void release_context(ImGuiContext* context);

    private:
        ImFont* load_variation(ImGuiContext* context, int size);

        std::filesystem::path m_font_location;
        std::unordered_map<ImGuiContext*, ContextFonts> m_contexts;
        std::unique_ptr<ImFontConfig> m_cfg;
    };

    class FontRegistry final : public AssetRegistry {
    public:
        Font* add(std::string id, std::filesystem::path location);
        Font* find(std::string_view id);
        const Font* find(std::string_view id) const;
    };
} // namespace ui
