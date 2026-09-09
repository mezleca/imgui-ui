#pragma once

#include "asset-registry.hpp"

#include <filesystem>
#include <imgui.h>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace ui {
    /// stores decoded image data and creates context-owned gpu data on demand.
    class Texture {
    public:
        virtual ~Texture() = default;
        virtual ImVec2 size() const = 0;

        // creates gpu data on the first draw for each imgui context and reuses it on later draws.
        virtual ImTextureID get(ImVec2 size) = 0;

        // removes one context's gpu data while keeping the decoded cpu source for another context.
        virtual void release_context(ImGuiContext* context) = 0;
    };

    class TextureLoader {
    public:
        virtual ~TextureLoader() = default;

        // decodes the source during registration without requiring an imgui or graphics context.
        virtual std::unique_ptr<Texture> load(const std::filesystem::path& location, std::string id) = 0;
        virtual std::unique_ptr<Texture> load(std::string_view content, std::string id) = 0;
    };

    // add() stores one decoded texture per id. drawing calls get() so each context creates its gpu object only when the
    // texture becomes visible. runtime releases that object before destroying a context and keeps the decoded source for
    // later contexts.
    class TextureRegistry final : public AssetRegistry {
    public:
        explicit TextureRegistry(std::unique_ptr<TextureLoader> loader = nullptr);

        Texture* add(std::string id, const std::filesystem::path& location);
        Texture* add(std::string id, std::string_view content);
        Texture* find(std::string_view id);
        const Texture* find(std::string_view id) const;

    private:
        template <typename Source>
        Texture* load_asset(std::string id, Source&& source) {
            if (Texture* existing = find(id); existing != nullptr) {
                return existing;
            }

            if (m_loader == nullptr) {
                return nullptr;
            }

            auto texture = m_loader->load(std::forward<Source>(source), id);
            return add_asset(std::move(id), std::move(texture));
        }

        std::unique_ptr<TextureLoader> m_loader;
    };
} // namespace ui
