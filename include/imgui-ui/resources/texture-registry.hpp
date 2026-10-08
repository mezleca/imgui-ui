#pragma once

#include "asset-registry.hpp"

#include <cstdio>
#include <filesystem>
#include <imgui.h>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace ui {
    class Texture {
    public:
        virtual ~Texture() = default;
        virtual ImVec2 size() const = 0;

        // the opengl loader creates gpu data lazily for each imgui context and reuses it later.
        virtual ImTextureID get(ImVec2 size) = 0;

        // release context-local data while retaining the source for later contexts.
        virtual void release_context(ImGuiContext* context) = 0;
    };

    class TextureLoader {
    public:
        virtual ~TextureLoader() = default;

        // decodes the source during registration without requiring an imgui or graphics context.
        virtual std::unique_ptr<Texture> load(const std::filesystem::path& location, std::string id) = 0;
        virtual std::unique_ptr<Texture> load(std::string_view content, std::string id) = 0;
    };

    class TextureRegistry final : public AssetRegistry<Texture> {
    public:
        explicit TextureRegistry(std::unique_ptr<TextureLoader> loader = nullptr);

        Texture* add(std::string id, const std::filesystem::path& location);
        Texture* add(std::string id, std::string_view content);

    private:
        template <typename Source>
        Texture* load_asset(std::string id, Source&& source) {
            Texture* existing = find(id);
            if (existing != nullptr) {
                return existing;
            }

            if (m_loader == nullptr) {
                std::fprintf(stderr, "imgui-ui: cannot load texture '%s' without a TextureLoader\n", id.c_str());
                return nullptr;
            }

            auto texture = m_loader->load(std::forward<Source>(source), id);
            return add_asset(std::move(id), std::move(texture));
        }

        std::unique_ptr<TextureLoader> m_loader;
    };
} // namespace ui
