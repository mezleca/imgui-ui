#pragma once

#include "asset.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace ui {
    class AssetRegistry {
    public:
        AssetRegistry() = default;
        virtual ~AssetRegistry() = default;

        AssetRegistry(const AssetRegistry&) = delete;
        AssetRegistry& operator=(const AssetRegistry&) = delete;

        void release_context(ImGuiContext* context);

    protected:
        template <typename T>
        T* add_asset(std::string id, std::unique_ptr<T> asset) {
            if (asset == nullptr) {
                return nullptr;
            }

            if (const auto existing = m_assets.find(id); existing != m_assets.end()) {
                return dynamic_cast<T*>(existing->second.get());
            }

            T* result = asset.get();
            m_assets.emplace(std::move(id), std::move(asset));
            return result;
        }

        template <typename T>
        T* find_asset(std::string_view id) {
            const auto result = m_assets.find(std::string{id});
            return result == m_assets.end() ? nullptr : dynamic_cast<T*>(result->second.get());
        }

        template <typename T>
        const T* find_asset(std::string_view id) const {
            const auto result = m_assets.find(std::string{id});
            return result == m_assets.end() ? nullptr : dynamic_cast<const T*>(result->second.get());
        }

    private:
        std::unordered_map<std::string, std::unique_ptr<Asset>> m_assets;
    };
} // namespace ui
