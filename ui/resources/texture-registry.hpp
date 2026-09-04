#pragma once

#include "asset-registry.hpp"

#include <filesystem>
#include <imgui.h>
#include <memory>
#include <string>
#include <string_view>

namespace ui {
    class Texture {
    public:
        virtual ~Texture() = default;
        virtual ImTextureID get(ImVec2 size) = 0;
        virtual void release_context(ImGuiContext* context) = 0;
    };

    class TextureLoader {
    public:
        virtual ~TextureLoader() = default;
        virtual std::unique_ptr<Texture> load(const std::filesystem::path& location, std::string id) = 0;
        virtual std::unique_ptr<Texture> load(std::string_view content, std::string id) = 0;
    };

    class TextureRegistry final : public AssetRegistry {
    public:
        explicit TextureRegistry(std::unique_ptr<TextureLoader> loader = nullptr);

        Texture* add(std::string id, std::filesystem::path location);
        Texture* find(std::string_view id);
        const Texture* find(std::string_view id) const;

    private:
        std::unique_ptr<TextureLoader> m_loader;
    };
} // namespace ui
