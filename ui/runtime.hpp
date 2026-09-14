#pragma once

#include "resources/font-registry.hpp"
#include "resources/texture-registry.hpp"
#include "style/theme.hpp"

#include <filesystem>
#include <memory>

namespace ui {
    class UI;

    struct RuntimeConfig {
        Theme theme{};
        std::filesystem::path performance_directory;
        std::unique_ptr<TextureLoader> texture_loader;
    };

    /// owns assets and visual defaults shared by every UI surface created from it.
    ///
    /// Runtime outlives its surfaces so fonts and textures can safely cache data for each surface's ImGui context.
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
        friend class UI;

        void set_theme(const Theme& theme) {
            m_theme = theme;
        }

        void release_context(ImGuiContext* context);

        Theme m_theme;
        FontRegistry m_fonts;
        TextureRegistry m_textures;
        std::filesystem::path m_performance_directory;
    };
} // namespace ui
