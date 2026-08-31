#include "assets.hpp"
#include "svg.hpp"

#include <iostream>
#include <stdexcept>
#include <utility>

namespace ui {
    AssetRegistry::AssetRegistry(std::unique_ptr<IconLoader> icon_loader) : m_icon_loader(std::move(icon_loader)) {
        if (m_icon_loader != nullptr) {
            m_textures.emplace("default", m_icon_loader->load_data(DEFAULT_WARN_SVG, "default"));
            m_textures.emplace(
                "context-menu-chevron", m_icon_loader->load_data(CONTEXT_MENU_CHEVRON_SVG, "context-menu-chevron")
            );
        }
    }

    Font* AssetRegistry::add_font(FontType type, std::filesystem::path location) {
        const std::size_t index = static_cast<size_t>(type);
        if (index >= m_fonts.size()) {
            return nullptr;
        }

        if (m_fonts[index] != nullptr) {
            return m_fonts[index].get();
        }

        auto font = std::make_unique<Font>();
        ImFontConfig config;
        config.PixelSnapH = false;
        config.OversampleH = 5;
        config.OversampleV = 5;
        config.RasterizerMultiply = 1.2f;
        font->initialize(config, std::move(location));

        Font* result = font.get();
        m_fonts[index] = std::move(font);
        return result;
    }

    Font& AssetRegistry::font(FontType type) {
        const std::size_t index = static_cast<size_t>(type);
        if (index >= m_fonts.size() || m_fonts[index] == nullptr) {
            throw std::runtime_error("ui: requested font was not registered");
        }

        return *m_fonts[index];
    }

    const Font& AssetRegistry::font(FontType type) const {
        const std::size_t index = static_cast<size_t>(type);
        if (index >= m_fonts.size() || m_fonts[index] == nullptr) {
            throw std::runtime_error("ui: requested font was not registered");
        }

        return *m_fonts[index];
    }

    Font* AssetRegistry::find_font(FontType type) {
        const std::size_t index = static_cast<size_t>(type);
        return index < m_fonts.size() ? m_fonts[index].get() : nullptr;
    }

    IconTexture* AssetRegistry::add_resource(std::string id, std::filesystem::path location) {
        if (m_icon_loader == nullptr) {
            return nullptr;
        }

        auto resource = m_icon_loader->load_file(location, id);
        IconTexture* result = resource.get();
        m_textures.insert_or_assign(std::move(id), std::move(resource));
        return result;
    }

    IconTexture* AssetRegistry::texture(std::string_view id) {
        IconTexture* result = find_texture(id);
        if (result == nullptr) {
            std::cout << "[ui] failed to find " << id << " (returning default svg)\n";
            return find_texture("default");
        }

        return result;
    }

    IconTexture* AssetRegistry::find_texture(std::string_view id) {
        const auto it = m_textures.find(std::string{id});
        return it == m_textures.end() ? nullptr : it->second.get();
    }

    void AssetRegistry::release_context(ImGuiContext* context) {
        for (const auto& font : m_fonts) {
            if (font != nullptr) {
                font->release_context(context);
            }
        }

        for (auto& entry : m_textures) {
            entry.second->release_context(context);
        }
    }

} // namespace ui
