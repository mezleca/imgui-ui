#include "asset-registry.hpp"

using namespace ui;
void AssetRegistry::release_context(ImGuiContext* context) {
    for (const auto& entry : m_assets) {
        if (entry.second != nullptr) {
            entry.second->release_context(context);
        }
    }
}
