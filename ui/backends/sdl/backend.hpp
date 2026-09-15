#pragma once

#include "../backend.hpp"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>

namespace ui {
    class UI;

    class SdlBackend final : public Backend {
    public:
        /// uses the application's existing SDL OpenGL window and context.
        /// both must outlive this backend and stay valid until after UI destruction.
        SdlBackend(SDL_Window* window, SDL_GLContext context);
        ~SdlBackend() override;

        bool initialize() override;
        void process_events(UI& surface) override;
        void register_effects(EffectRegistry& effects) override;
        bool initialize_imgui() override;
        void shutdown_imgui() override;
        void begin_frame(ImVec4 clear_color) override;
        void set_mouse_cursor(ImGuiMouseCursor cursor) override;
        void render(ImDrawData* draw_data) override;
        float content_scale() const override;
        uint64_t window_id() const override;
        ImVec2 display_size() const override;

        /// forwards one sdl event to the retained tree and imgui.
        bool process_event(UI& surface, const SDL_Event& event);

    private:
        void apply_mouse_cursor(ImGuiMouseCursor cursor);

        SDL_Window* m_window = nullptr;
        SDL_GLContext m_context = nullptr;
        SDL_Cursor* m_mouse_cursor = nullptr;
        ImGuiMouseCursor m_mouse_cursor_type = ImGuiMouseCursor_Arrow;
        bool m_imgui_initialized = false;
    };

} // namespace ui
