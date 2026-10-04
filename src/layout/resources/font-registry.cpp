#include <imgui-ui/resources/font-registry.hpp>

#include <utility>

using namespace ui;

Font::Font(std::filesystem::path location, ImFontConfig config) : m_location(std::move(location)), m_config(config) {}

ImFont* Font::get(int size) {
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr || m_location.empty()) {
        return nullptr;
    }

    auto& fonts = m_contexts[context];
    const auto font_it = fonts.find(size);
    if (font_it != fonts.end()) {
        return font_it->second;
    }

    ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(m_location.string().c_str(), static_cast<float>(size), &m_config);
    if (font != nullptr) {
        fonts[size] = font;
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
