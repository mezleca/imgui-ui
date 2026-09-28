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

Texture* TextureRegistry::add(std::string id, const std::filesystem::path& location) {
    return load_asset(std::move(id), location);
}

Texture* TextureRegistry::add(std::string id, std::string_view content) {
    return load_asset(std::move(id), content);
}
