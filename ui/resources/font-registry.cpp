#include "font-registry.hpp"

#include <iostream>
#include <utility>

using namespace ui;
Font::Font(std::filesystem::path location) : m_font_location(std::move(location)) {
    m_cfg.PixelSnapH = false;
    m_cfg.OversampleH = 5;
    m_cfg.OversampleV = 5;
    m_cfg.RasterizerMultiply = 1.2F;
}

ImFont* Font::load_variation(ImGuiContext* context, int size) {
    if (context == nullptr || m_font_location.empty()) {
        return nullptr;
    }

    std::cout << "[ui] loading " << m_font_location << " (" << size << ")\n";

    ContextFonts& context_fonts = m_contexts[context];
    ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(m_font_location.string().c_str(), static_cast<float>(size), &m_cfg);

    if (font != nullptr) {
        context_fonts.fonts[size] = font;
    }

    return font;
}

ImFont* Font::get(int size) {
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr) {
        return nullptr;
    }

    ContextFonts& context_fonts = m_contexts[context];
    const auto font_it = context_fonts.fonts.find(size);
    if (font_it != context_fonts.fonts.end()) {
        return font_it->second;
    }

    return load_variation(context, size);
}

void Font::release_context(ImGuiContext* context) {
    if (context != nullptr) {
        m_contexts.erase(context);
    }
}

Font* FontRegistry::add(std::string id, std::filesystem::path location) {
    return add_asset(std::move(id), std::make_unique<Font>(std::move(location)));
}

Font* FontRegistry::find(std::string_view id) {
    return find_asset<Font>(id);
}

const Font* FontRegistry::find(std::string_view id) const {
    return find_asset<Font>(id);
}
