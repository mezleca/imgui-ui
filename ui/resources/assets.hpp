#pragma once

#include "font.hpp"
#include "icon.hpp"

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace ui {
    class AssetRegistry {
    public:
        explicit AssetRegistry(std::unique_ptr<IconLoader> icon_loader = nullptr);

        AssetRegistry(const AssetRegistry&) = delete;
        AssetRegistry& operator=(const AssetRegistry&) = delete;

        Font* add_font(FontType type, std::filesystem::path location);
        Font* find_font(FontType type);
        Font& font(FontType type);
        const Font& font(FontType type) const;
        IconTexture* add_resource(std::string id, std::filesystem::path location);
        IconTexture* find_texture(std::string_view id);
        void release_context(ImGuiContext* context);

    private:
        std::array<std::unique_ptr<Font>, static_cast<size_t>(FontType::FONT_COUNT)> m_fonts;
        std::unique_ptr<IconLoader> m_icon_loader;
        std::unordered_map<std::string, std::unique_ptr<IconTexture>> m_textures;
    };
} // namespace ui
