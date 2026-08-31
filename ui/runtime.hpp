#pragma once

#include "resources/assets.hpp"
#include "style/theme.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

class UI;

namespace ui {
    struct RuntimeConfig {
        Theme theme = Theme::defaults();
        std::filesystem::path performance_directory;
        std::unique_ptr<IconLoader> icon_loader;
    };

    /// assets and theme shared by independent ui surfaces.
    class Runtime {
    public:
        explicit Runtime(RuntimeConfig config = {});

        Runtime(const Runtime&) = delete;
        Runtime& operator=(const Runtime&) = delete;

        const Theme& theme() const {
            return m_theme;
        }

        Font* add_font(FontType type, std::filesystem::path location) {
            return m_assets.add_font(type, std::move(location));
        }

        IconTexture* add_resource(std::string id, std::filesystem::path location) {
            return m_assets.add_resource(std::move(id), std::move(location));
        }

        Font* find_font(FontType type) {
            return m_assets.find_font(type);
        }

        Font& font(FontType type) {
            return m_assets.font(type);
        }

        const Font& font(FontType type) const {
            return m_assets.font(type);
        }

        IconTexture* resource(std::string_view id) {
            return m_assets.texture(id);
        }

        IconTexture* find_resource(std::string_view id) {
            return m_assets.find_texture(id);
        }

        const std::filesystem::path& performance_directory() const {
            return m_performance_directory;
        }

    private:
        friend class ::UI;

        void release_context(ImGuiContext* context);

        Theme m_theme;
        AssetRegistry m_assets;
        std::filesystem::path m_performance_directory;
    };
} // namespace ui
