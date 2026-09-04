#include "font-registry.hpp"

#include <utility>

using namespace ui;
Font* FontRegistry::add(std::string id, std::filesystem::path location) {
    if (Font* existing = find(id); existing != nullptr) {
        return existing;
    }

    auto font = std::make_unique<Font>();
    ImFontConfig config;
    config.PixelSnapH = false;
    config.OversampleH = 5;
    config.OversampleV = 5;
    config.RasterizerMultiply = 1.2F;
    font->initialize(config, std::move(location));
    return add_asset(std::move(id), std::move(font));
}

Font* FontRegistry::find(std::string_view id) {
    return find_asset<Font>(id);
}

const Font* FontRegistry::find(std::string_view id) const {
    return find_asset<Font>(id);
}
