#pragma once

#include "../../backend.hpp"

#include <memory>

class UI;

namespace ui {
    class RaylibBackend final : public Backend {
    public:
        explicit RaylibBackend(Config config);
        explicit RaylibBackend(bool attach_to_current_window);
        ~RaylibBackend() override;

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

        bool process_events(UI& surface);

    private:
        Config m_config;
        ImVec2 m_previous_mouse_position{};
        bool m_attached = false;
        bool m_owns_window = false;
        bool m_imgui_initialized = false;
        bool m_has_mouse_position = false;
    };

    std::unique_ptr<Backend> create_raylib_backend(const Config& config);
    std::unique_ptr<Backend> attach_raylib_backend();
    bool process_raylib_events(UI& surface);
} // namespace ui
