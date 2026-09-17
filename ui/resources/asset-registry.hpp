#pragma once

#include "../string-hash.hpp"

#include <imgui.h>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace ui {
    template <typename T>
    class AssetRegistry {
    public:
        AssetRegistry() = default;
        AssetRegistry(const AssetRegistry&) = delete;
        AssetRegistry& operator=(const AssetRegistry&) = delete;

        void release_context(ImGuiContext* context) {
            for (const auto& entry : m_assets) {
                entry.second->release_context(context);
            }
        }

    protected:
        T* add_asset(std::string id, std::unique_ptr<T> asset) {
            if (asset == nullptr) {
                return nullptr;
            }

            if (const auto existing = m_assets.find(id); existing != m_assets.end()) {
                return existing->second.get();
            }

            T* result = asset.get();
            m_assets.emplace(std::move(id), std::move(asset));
            return result;
        }

        T* find_asset(std::string_view id) {
            return const_cast<T*>(static_cast<const AssetRegistry&>(*this).find_asset(id));
        }

        const T* find_asset(std::string_view id) const {
            const auto result = m_assets.find(id);
            if (result == m_assets.end()) {
                return nullptr;
            }

            return result->second.get();
        }

    private:
        std::unordered_map<std::string, std::unique_ptr<T>, StringHash, std::equal_to<>> m_assets;
    };
} // namespace ui
