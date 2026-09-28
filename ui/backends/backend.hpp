#pragma once

#include <imgui.h>
#include "../style/color.hpp"

#include <cstdint>
#include <optional>

namespace ui {
    class EffectRegistry;
    class UI;

    /// connects one UI surface to its platform input and rendering context.
    class Backend {
    public:
        Backend() = default;
        virtual ~Backend() = default;

        Backend(const Backend&) = delete;
        Backend& operator=(const Backend&) = delete;

        /// checks that the application's platform window and graphics context are ready.
        virtual bool initialize() = 0;
        /// forwards platform input collected since the preceding rendered frame.
        virtual void process_events(UI&) = 0;
        virtual void register_effects(EffectRegistry&) {}
        virtual bool initialize_imgui() = 0;
        virtual void shutdown_imgui() = 0;
        /// starts the platform draw cycle and clears the target before UI::draw().
        virtual void begin_frame(Color clear_color) = 0;
        virtual void set_mouse_cursor(ImGuiMouseCursor cursor) = 0;
        /// submits imgui draw data and completes the platform draw cycle.
        virtual void render(ImDrawData* draw_data) = 0;
        /// returns the latest completed gpu query from rendering imgui draw data, if available.
        virtual std::optional<double> render_profiled(ImDrawData* draw_data, bool) {
            render(draw_data);
            return std::nullopt;
        }
        virtual float content_scale() const = 0;
        virtual uint64_t window_id() const = 0;
        virtual ImVec2 display_size() const = 0;
    };
} // namespace ui
