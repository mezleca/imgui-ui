#include <imgui-ui/resources/font-registry.hpp>

#include <utility>

using namespace ui;

Font::Font(std::filesystem::path location, ImFontConfig config) : m_location(std::move(location)), m_config(config) {}

ImFont* Font::get() {
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr || m_location.empty()) {
        return nullptr;
    }

    const auto font_it = m_contexts.find(context);
    if (font_it != m_contexts.end()) {
        return font_it->second;
    }

    ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(m_location.string().c_str(), 0.0F, &m_config);
    if (font != nullptr) {
        m_contexts[context] = font;
    }
    return font;
}

void Font::release_context(ImGuiContext* context) {
    m_contexts.erase(context);
}

Font* FontRegistry::add(std::string id, std::filesystem::path location) {
    ImFontConfig config;
    config.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_LightHinting;
    return add(std::move(id), std::move(location), config);
}

Font* FontRegistry::add(std::string id, std::filesystem::path location, ImFontConfig config) {
    return add_asset(std::move(id), std::make_unique<Font>(std::move(location), config));
}
