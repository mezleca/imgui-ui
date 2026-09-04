#pragma once

#include "resources/font-registry.hpp"
#include "resources/texture-registry.hpp"
#include "style/theme.hpp"

#include <filesystem>
#include <memory>
#include <utility>

class UI;

namespace ui {
    struct RuntimeConfig {
        Theme theme = Theme::defaults();
        std::filesystem::path performance_directory;
        std::unique_ptr<TextureLoader> texture_loader;
    };

    /// assets and theme shared by independent ui surfaces.
    class Runtime {
    public:
        explicit Runtime(RuntimeConfig config = {});

        Runtime(const Runtime&) = delete;
        Runtime& operator=(const Runtime&) = delete;

        const Theme& theme() const {
            return m_theme;
        }

        FontRegistry& fonts() {
            return m_fonts;
        }

        const FontRegistry& fonts() const {
            return m_fonts;
        }

        TextureRegistry& textures() {
            return m_textures;
        }

        const TextureRegistry& textures() const {
            return m_textures;
        }

        const std::filesystem::path& performance_directory() const {
            return m_performance_directory;
        }

    private:
        friend class ::UI;

        void set_theme(Theme theme) {
            m_theme = std::move(theme);
        }

        void release_context(ImGuiContext* context);

        Theme m_theme;
        FontRegistry m_fonts;
        TextureRegistry m_textures;
        std::filesystem::path m_performance_directory;
    };
} // namespace ui
