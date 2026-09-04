#pragma once

#include <imgui.h>

namespace ui {
    class Asset {
    public:
        virtual ~Asset() = default;
        virtual void release_context(ImGuiContext*) {}
    };
} // namespace ui
