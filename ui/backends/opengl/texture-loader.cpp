#include "texture-loader.hpp"

#include <glad/gl.h>
#include <lunasvg.h>
#include <vendor/lunasvg/plutovg/include/plutovg.h>

#include <format>
#include <stdexcept>
#include <unordered_map>
#include <utility>

using namespace ui;
class OpenGLTexture final : public Texture {
public:
    explicit OpenGLTexture(std::unique_ptr<lunasvg::Document> document) : m_document(std::move(document)) {}

    ImVec2 size() const override {
        return {m_document->width(), m_document->height()};
    }

    ImTextureID get(ImVec2 size) override {
        ImGuiContext* context = ImGui::GetCurrentContext();
        if (context == nullptr) {
            return {};
        }

        const int width = static_cast<int>(size.x);
        const int height = static_cast<int>(size.y);
        const uint64_t size_key = (static_cast<uint64_t>(static_cast<uint32_t>(width)) << 32) | static_cast<uint32_t>(height);
        BitmapCache& cache = m_bitmaps[context];
        const auto existing = cache.find(size_key);
        if (existing != cache.end()) {
            return static_cast<ImTextureID>(existing->second.first);
        }

        lunasvg::Bitmap bitmap_data = m_document->renderToBitmap(width, height);
        bitmap_data.convertToRGBA();
        auto bitmap = std::make_unique<lunasvg::Bitmap>(bitmap_data);

        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap->data());

        cache.emplace(size_key, std::make_pair(texture, std::move(bitmap)));
        return static_cast<ImTextureID>(texture);
    }

    void release_context(ImGuiContext* context) override {
        const auto found = m_bitmaps.find(context);
        if (found == m_bitmaps.end()) {
            return;
        }

        for (const auto& entry : found->second) {
            const GLuint id = entry.second.first;
            glDeleteTextures(1, &id);
        }
        m_bitmaps.erase(found);
    }

private:
    using BitmapCache = std::unordered_map<uint64_t, std::pair<GLuint, std::unique_ptr<lunasvg::Bitmap>>>;

    std::unordered_map<ImGuiContext*, BitmapCache> m_bitmaps;
    std::unique_ptr<lunasvg::Document> m_document;
};

class OpenGLRasterTexture final : public Texture {
public:
    explicit OpenGLRasterTexture(plutovg_surface_t* surface) : m_surface(surface) {}

    ~OpenGLRasterTexture() override {
        plutovg_surface_destroy(m_surface);
    }

    ImVec2 size() const override {
        return {
            static_cast<float>(plutovg_surface_get_width(m_surface)),
            static_cast<float>(plutovg_surface_get_height(m_surface)),
        };
    }

    ImTextureID get(ImVec2) override {
        ImGuiContext* context = ImGui::GetCurrentContext();
        if (context == nullptr) {
            return {};
        }

        const auto existing = m_textures.find(context);
        if (existing != m_textures.end()) {
            return static_cast<ImTextureID>(existing->second);
        }

        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGBA, plutovg_surface_get_width(m_surface), plutovg_surface_get_height(m_surface), 0, GL_BGRA,
            GL_UNSIGNED_BYTE, plutovg_surface_get_data(m_surface)
        );

        m_textures.emplace(context, texture);
        return static_cast<ImTextureID>(texture);
    }

    void release_context(ImGuiContext* context) override {
        const auto found = m_textures.find(context);
        if (found == m_textures.end()) {
            return;
        }

        glDeleteTextures(1, &found->second);
        m_textures.erase(found);
    }

private:
    plutovg_surface_t* m_surface = nullptr;
    std::unordered_map<ImGuiContext*, GLuint> m_textures;
};

std::unique_ptr<Texture> OpenGLTextureLoader::load(const std::filesystem::path& location, std::string) {
    if (location.extension() != ".svg") {
        plutovg_surface_t* surface = plutovg_surface_load_from_image_file(location.string().c_str());
        if (surface == nullptr) {
            throw std::runtime_error(std::format("failed to load texture {}", location.string()));
        }
        return std::make_unique<OpenGLRasterTexture>(surface);
    }

    std::unique_ptr<lunasvg::Document> document = lunasvg::Document::loadFromFile(location.string());
    if (document == nullptr) {
        throw std::runtime_error(std::format("failed to load texture {}", location.string()));
    }
    return std::make_unique<OpenGLTexture>(std::move(document));
}

std::unique_ptr<Texture> OpenGLTextureLoader::load(std::string_view content, std::string) {
    std::unique_ptr<lunasvg::Document> document = lunasvg::Document::loadFromData(std::string{content});
    if (document == nullptr) {
        throw std::runtime_error("failed to load texture data");
    }
    return std::make_unique<OpenGLTexture>(std::move(document));
}
