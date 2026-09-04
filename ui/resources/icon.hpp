#pragma once

#include "asset.hpp"

#include <imgui.h>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

class IconTexture : public ui::Asset {
public:
    virtual ~IconTexture() = default;
    virtual ImTextureID get(ImVec2 size) = 0;
    void release_context(ImGuiContext* context) override = 0;
};

namespace ui {
    class IconLoader {
    public:
        virtual ~IconLoader() = default;
        virtual std::unique_ptr<IconTexture> load_file(const std::filesystem::path& location, std::string id) = 0;
        virtual std::unique_ptr<IconTexture> load_data(std::string_view content, std::string id) = 0;
    };
} // namespace ui
