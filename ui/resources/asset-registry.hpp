#pragma once

#include <imgui.h>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace ui {
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
        template <typename T>
        T* add_asset(std::string id, std::unique_ptr<T> asset) {
            if (asset == nullptr) {
                return nullptr;
            }

            if (const auto existing = m_assets.find(id); existing != m_assets.end()) {
                auto* typed = dynamic_cast<AssetEntry<T>*>(existing->second.get());
                return typed == nullptr ? nullptr : typed->value.get();
            }

            auto entry = std::make_unique<AssetEntry<T>>(std::move(asset));
            T* result = entry->value.get();
            m_assets.emplace(std::move(id), std::move(entry));
            return result;
        }

        template <typename T>
        T* find_asset(std::string_view id) {
            return const_cast<T*>(static_cast<const AssetRegistry&>(*this).find_asset<T>(id));
        }

        template <typename T>
        const T* find_asset(std::string_view id) const {
            const auto result = m_assets.find(std::string{id});
            if (result == m_assets.end()) {
                return nullptr;
            }

            const auto* typed = dynamic_cast<const AssetEntry<T>*>(result->second.get());
            return typed == nullptr ? nullptr : typed->value.get();
        }

    private:
        struct AssetEntryBase {
            virtual ~AssetEntryBase() = default;
            virtual void release_context(ImGuiContext* context) = 0;
        };

        template <typename T>
        struct AssetEntry final : AssetEntryBase {
            explicit AssetEntry(std::unique_ptr<T> value) : value(std::move(value)) {}

            void release_context(ImGuiContext* context) override {
                value->release_context(context);
            }

            std::unique_ptr<T> value;
        };

        std::unordered_map<std::string, std::unique_ptr<AssetEntryBase>> m_assets;
    };
} // namespace ui
