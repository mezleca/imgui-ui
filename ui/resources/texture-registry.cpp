#include "texture-registry.hpp"

#include "svg.hpp"

#include <utility>

using namespace ui;
TextureRegistry::TextureRegistry(std::unique_ptr<IconLoader> loader) : m_loader(std::move(loader)) {
    if (m_loader == nullptr) {
        return;
    }

    add_asset("default", m_loader->load_data(DEFAULT_WARN_SVG, "default"));
    add_asset("context-menu-chevron", m_loader->load_data(CONTEXT_MENU_CHEVRON_SVG, "context-menu-chevron"));
}

IconTexture* TextureRegistry::add(std::string id, std::filesystem::path location) {
    if (IconTexture* existing = find(id); existing != nullptr) {
        return existing;
    }

    if (m_loader == nullptr) {
        return nullptr;
    }

    auto texture = m_loader->load_file(location, id);
    return add_asset(std::move(id), std::move(texture));
}

IconTexture* TextureRegistry::find(std::string_view id) {
    return find_asset<IconTexture>(id);
}

const IconTexture* TextureRegistry::find(std::string_view id) const {
    return find_asset<IconTexture>(id);
}
