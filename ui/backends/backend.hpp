#pragma once

#include <imgui.h>

#include <cstdint>
#include <string>
#include <utility>

namespace ui {
    class EffectRegistry;

    struct BackendConfig {
        std::string title = "ui";
        ImVec2 size{};
        bool resizable = false;
        bool visible = true;
        int swap_interval = 1;
    };

    class Backend {
    public:
        explicit Backend(BackendConfig config = {}) : m_config(std::move(config)) {}
        virtual ~Backend() = default;

        Backend(const Backend&) = delete;
        Backend& operator=(const Backend&) = delete;

        const BackendConfig& config() const {
            return m_config;
        }

        virtual bool initialize() = 0;
        virtual void register_effects(EffectRegistry&) {}
        virtual bool initialize_imgui() = 0;
        virtual void shutdown_imgui() = 0;
        virtual void begin_frame(ImVec4 clear_color) = 0;
        virtual void set_mouse_cursor(ImGuiMouseCursor cursor) = 0;
        virtual void render(ImDrawData* draw_data) = 0;
        virtual float content_scale() const = 0;
        virtual uint64_t window_id() const = 0;
        virtual ImVec2 display_size() const = 0;

    protected:
        BackendConfig& config() {
            return m_config;
        }

    private:
        BackendConfig m_config;
    };
} // namespace ui
