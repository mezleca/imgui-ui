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

class UI;

namespace ui {
    class Debugger;
    class Node;

    struct UIConfig {
        std::unique_ptr<Backend> backend;
        /// creates debugger support. the overlay stays hidden until its hotkey opens it.
        bool enable_debugger = false;
    };
} // namespace ui

/// assets and theme remain owned by runtime.
/// imgui context, root and router are surface-local.
class UI {
public:
    explicit UI(ui::Runtime& runtime, ui::UIConfig config = {});
    ~UI();

    UI(const UI&) = delete;
    UI& operator=(const UI&) = delete;

    void exit() {
        m_done = true;
    }

    /// clears the previous frame's input entries and starts imgui.
    /// call after forwarding platform events and before updating the root.
    /// the configured debugger hotkey is processed here.
    void begin_frame();

    /// draws the configured debugger, presents the imgui frame, then restores the previous context.
    void end_frame();

    bool dispatch(ui::UiEvent& event);

    bool is_done() const {
        return m_done;
    }

    bool ready() const {
        return m_ready;
    }

    /// returns a registered font variation, or imgui's current font when it is unavailable.
    ImFont* get_font(std::string_view id, int size) const;

    void set_primary_font(ui::Font* font) {
        m_primary_font = font;
    }

    ImFont* get_primary_font(int size) const {
        return resolve_font(m_primary_font, size);
    }

    void set_secondary_font(ui::Font* font) {
        m_secondary_font = font;
    }

    ImFont* get_secondary_font(int size) const {
        return resolve_font(m_secondary_font, size);
    }

    ui::InputRouter& input_router() {
        return m_input_router;
    }

    ui::Profiler& profiler() {
        return m_profiler;
    }

    const ui::Profiler& profiler() const {
        return m_profiler;
    }

    /// returns the debugger when diagnostic support was configured, or nullptr otherwise.
    ui::Debugger* debugger() {
        return m_debugger.get();
    }

    const ui::Debugger* debugger() const {
        return m_debugger.get();
    }

    bool debugger_blocks_pointer_input() const;

    ui::EffectRegistry& effects() {
        return m_effects;
    }

    const ui::EffectRegistry& effects() const {
        return m_effects;
    }

    const ui::Theme& theme() const {
        return m_runtime.theme();
    }

    /// updates imgui colors and reapplies theme defaults across the retained tree.
    void set_theme(ui::Theme theme);

    ui::Runtime& runtime() {
        return m_runtime;
    }

    /// returns the retained root that owns application nodes.
    ui::Node& root() {
        return *m_root;
    }

    ui::Backend& backend() {
        return *m_backend;
    }

    ImGuiContext* imgui_context() {
        return m_context;
    }

private:
    ImFont* resolve_font(ui::Font* font, int size) const;
    void initialize();
    void configure_style(float main_scale);
    void apply_theme_colors();

    ui::Runtime& m_runtime;
    ImGuiContext* m_context = nullptr;
    ImGuiContext* m_previous_context = nullptr;
    std::unique_ptr<ui::Backend> m_backend;
    std::unique_ptr<ui::Node> m_root;
    ui::InputRouter m_input_router;
    ui::EffectRegistry m_effects;
    ui::Profiler m_profiler;
    std::unique_ptr<ui::Debugger> m_debugger;
    ui::Font* m_primary_font = nullptr;
    ui::Font* m_secondary_font = nullptr;
    bool m_done = false;
    bool m_ready = false;
};
