#pragma once

#include "../../backend.hpp"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>
#include <memory>

class UI;

namespace ui {
    class Window;

    class SdlBackend final : public Backend {
    public:
        explicit SdlBackend(Config config);
        SdlBackend(SDL_Window* window, SDL_GLContext context);
        ~SdlBackend() override;

        bool initialize() override;
        void register_effects(EffectRegistry& effects) override;
        bool initialize_imgui() override;
        void shutdown_imgui() override;
        void make_current() override;
        void begin_frame(ImVec4 clear_color) override;
        void set_mouse_cursor(ImGuiMouseCursor cursor) override;
        void render(ImDrawData* draw_data) override;
        float content_scale() const override;
        uint64_t window_id() const override;
        ImVec2 display_size() const override;
        bool focused() const override;
        void position_next_to(const Backend& target, float gap) override;
        void show() override;
        void hide() override;
        void raise() override;

        /// returns true only when the retained tree handles this native event.
        bool process_event(UI& surface, const SDL_Event& event);

    private:
        void apply_mouse_cursor(ImGuiMouseCursor cursor);

        Config m_config;
        std::unique_ptr<Window> m_window;
        SDL_Cursor* m_mouse_cursor = nullptr;
        ImGuiMouseCursor m_mouse_cursor_type = ImGuiMouseCursor_Arrow;
        bool m_attached = false;
        bool m_imgui_initialized = false;
    };

    std::unique_ptr<Backend> create_sdl_backend(const Config& config);
    std::unique_ptr<Backend> attach_sdl_backend(SDL_Window* window, SDL_GLContext context);
    bool process_sdl_event(UI& surface, const SDL_Event& event);
} // namespace ui
