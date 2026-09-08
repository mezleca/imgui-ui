#pragma once

#include "backends/backend.hpp"
#include "style/theme.hpp"
#include "diagnostics/profiler.hpp"
#include "imgui/effects/effects.hpp"
#include "input/router.hpp"
#include "runtime.hpp"

#include <imgui.h>
#include <memory>
#include <string_view>

namespace ui {
    class UI;
    class Debugger;
    class Node;
    class StackContainer;

    struct UIConfig {
        std::unique_ptr<Backend> backend;
        bool enable_debugger = false;
    };
    /// coordinates one application ui frame.
    class UI {
    public:
        explicit UI(ui::Runtime& runtime, ui::UIConfig config = {});
        ~UI();

        UI(const UI&) = delete;
        UI& operator=(const UI&) = delete;

        /// requests termination of the application's frame loop.
        void exit() {
            m_done = true;
        }

        /// starts an imgui frame and clears the previous frame's routed input.
        /// call after platform events and before update() or draw().
        void begin_frame();

        /// renders the current imgui frame and restores the previous context.
        void end_frame();

        /// updates the application tree for the current frame.
        void update(float dt);

        /// draws the application tree for the current frame.
        void draw();

        /// routes one platform event through the debugger and application tree.
        /// returns true when the event was handled or blocked.
        bool dispatch(ui::UiEvent& event);

        /// returns whether exit() was requested.
        bool is_done() const {
            return m_done;
        }

        /// returns whether backend, imgui, and the retained tree are ready.
        bool ready() const {
            return m_ready;
        }

        /// returns a registered font variation, or imgui's current font when it is unavailable.
        ImFont* get_font(std::string_view id, int size) const;

        /// sets the font inherited by widgets that use the primary font.
        void set_primary_font(ui::Font* font) {
            m_primary_font = font;
        }

        /// resolves a size from the primary font, falling back to imgui's current font.
        ImFont* get_primary_font(int size) const {
            return resolve_font(m_primary_font, size);
        }

        /// sets the font inherited by widgets that use the secondary font.
        void set_secondary_font(ui::Font* font) {
            m_secondary_font = font;
        }

        /// resolves a size from the secondary font, falling back to imgui's current font.
        ImFont* get_secondary_font(int size) const {
            return resolve_font(m_secondary_font, size);
        }

        /// returns the router used by the surface tree.
        ui::InputRouter& input_router() {
            return m_input_router;
        }

        /// returns frame timing and node instrumentation for this surface.
        ui::Profiler& profiler() {
            return m_profiler;
        }

        const ui::Profiler& profiler() const {
            return m_profiler;
        }

        /// returns the debugger when diagnostic support was configured, or nullptr otherwise.
        ui::Debugger* debugger() {
            return m_debugger;
        }

        const ui::Debugger* debugger() const {
            return m_debugger;
        }

        bool debugger_blocks_pointer_input() const;

        /// returns the effect registry used during frame rendering.
        ui::EffectRegistry& effects() {
            return m_effects;
        }

        const ui::EffectRegistry& effects() const {
            return m_effects;
        }

        const ui::Theme& theme() const {
            return m_runtime.theme();
        }

        /// updates imgui metrics and colors, then reapplies theme defaults across the retained tree.
        void set_theme(ui::Theme theme);

        ui::Runtime& runtime() {
            return m_runtime;
        }

        /// returns the application content root.
        ui::Node& root() {
            return *m_content_root;
        }

        /// returns the backend owned by this surface.
        ui::Backend& backend() {
            return *m_backend;
        }

        /// returns this surface's imgui context.
        ImGuiContext* imgui_context() {
            return m_context;
        }

    private:
        ImFont* resolve_font(ui::Font* font, int size) const;
        void initialize();
        void configure_style(float main_scale);
        void apply_theme_metrics();
        void apply_theme_colors();

        ui::Runtime& m_runtime;
        ImGuiContext* m_context = nullptr;
        ImGuiContext* m_previous_context = nullptr;
        std::unique_ptr<ui::Backend> m_backend;
        std::unique_ptr<ui::Node> m_root;
        ui::StackContainer* m_surface_layout = nullptr;
        ui::Node* m_content_root = nullptr;
        ui::InputRouter m_input_router;
        ui::EffectRegistry m_effects;
        ui::Profiler m_profiler;
        ui::Debugger* m_debugger = nullptr;
        ui::Font* m_primary_font = nullptr;
        ui::Font* m_secondary_font = nullptr;
        float m_content_scale = 1.0F;
        bool m_done = false;
        bool m_ready = false;
    };
} // namespace ui
