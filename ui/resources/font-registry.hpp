#pragma once

#include "asset-registry.hpp"
#include "font.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace ui {
    class FontRegistry final : public AssetRegistry {
    public:
        Font* add(std::string id, std::filesystem::path location);
        Font* find(std::string_view id);
        const Font* find(std::string_view id) const;
    };
} // namespace ui
