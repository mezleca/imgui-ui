#pragma once

#include "asset-registry.hpp"
#include "icon.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace ui {
    class TextureRegistry final : public AssetRegistry {
    public:
        explicit TextureRegistry(std::unique_ptr<IconLoader> loader = nullptr);

        IconTexture* add(std::string id, std::filesystem::path location);
        IconTexture* find(std::string_view id);
        const IconTexture* find(std::string_view id) const;

    private:
        std::unique_ptr<IconLoader> m_loader;
    };
} // namespace ui
