#pragma once

#include <imgui.h>

#include <cstdint>

namespace ui {
    class EffectRegistry;
    class UI;

    class Backend {
    public:
        Backend() = default;
        virtual ~Backend() = default;

        Backend(const Backend&) = delete;
        Backend& operator=(const Backend&) = delete;

        virtual bool initialize() = 0;
        virtual void process_events(UI&) = 0;
        virtual void register_effects(EffectRegistry&) {}
        virtual bool initialize_imgui() = 0;
        virtual void shutdown_imgui() = 0;
        virtual void begin_frame(ImVec4 clear_color) = 0;
        virtual void set_mouse_cursor(ImGuiMouseCursor cursor) = 0;
        virtual void render(ImDrawData* draw_data) = 0;
        virtual float content_scale() const = 0;
        virtual uint64_t window_id() const = 0;
        virtual ImVec2 display_size() const = 0;
    };
} // namespace ui
