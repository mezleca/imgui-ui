#pragma once

#include "../../backend.hpp"
#include <SDL3/SDL_events.h>
#include <memory>

class UI;

namespace ui {
    class Window;

    class SdlBackend final : public Backend {
    public:
        explicit SdlBackend(Config config);

        bool initialize() override;
        bool initialize_imgui() override;
        void shutdown_imgui() override;
        void make_current() override;
        void begin_frame(ImVec4 clear_color) override;
        void render(ImDrawData* draw_data) override;
        float content_scale() const override;
        uint64_t window_id() const override;
        ImVec2 display_size() const override;
        bool focused() const override;
        void position_next_to(const Backend& target, float gap) override;
        void show() override;
        void hide() override;
        void raise() override;

        bool process_event(UI& surface, const SDL_Event& event);

    private:
        Config m_config;
        std::unique_ptr<Window> m_window;
        bool m_imgui_initialized = false;
    };

    std::unique_ptr<Backend> create_sdl_backend(const Config& config);
    bool process_sdl_event(UI& surface, const SDL_Event& event);
} // namespace ui
