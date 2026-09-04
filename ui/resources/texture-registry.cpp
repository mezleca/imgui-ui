#include "texture-registry.hpp"

#include "svg.hpp"

#include <utility>

using namespace ui;
TextureRegistry::TextureRegistry(std::unique_ptr<TextureLoader> loader) : m_loader(std::move(loader)) {
    if (m_loader == nullptr) {
        return;
    }

    add_asset("default", m_loader->load(DEFAULT_WARN_SVG, "default"));
    add_asset("context-menu-chevron", m_loader->load(CONTEXT_MENU_CHEVRON_SVG, "context-menu-chevron"));
}

Texture* TextureRegistry::add(std::string id, std::filesystem::path location) {
    if (Texture* existing = find(id); existing != nullptr) {
        return existing;
    }

    if (m_loader == nullptr) {
        return nullptr;
    }

    auto texture = m_loader->load(location, id);
    return add_asset(std::move(id), std::move(texture));
}

Texture* TextureRegistry::add(std::string id, std::string_view content) {
    if (Texture* existing = find(id); existing != nullptr) {
        return existing;
    }

    if (m_loader == nullptr) {
        return nullptr;
    }

    auto texture = m_loader->load(content, id);
    return add_asset(std::move(id), std::move(texture));
}

Texture* TextureRegistry::find(std::string_view id) {
    return find_asset<Texture>(id);
}

const Texture* TextureRegistry::find(std::string_view id) const {
    return find_asset<Texture>(id);
}
