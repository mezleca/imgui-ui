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
    class Node;
} // namespace ui

/// assets and theme remain owned by runtime.
/// imgui context, root and router are surface-local.
class UI {
public:
    explicit UI(ui::Runtime& runtime, std::unique_ptr<ui::Backend> backend = nullptr);
    ~UI();

    UI(const UI&) = delete;
    UI& operator=(const UI&) = delete;

    void exit() {
        m_done = true;
    }

    /// clears this surface's per-frame input state before the next render pass.
    void begin_input_frame();

    /// makes this surface current and starts its imgui frame.
    /// call once before updating and drawing the root node.
    void begin_frame();

    /// presents the imgui frame, then restores the previous context.
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
        if (m_primary_font != nullptr) {
            if (ImFont* font = m_primary_font->get(size); font != nullptr) {
                return font;
            }
        }

        return ImGui::GetCurrentContext() == nullptr ? nullptr : ImGui::GetFont();
    }

    void set_secondary_font(ui::Font* font) {
        m_secondary_font = font;
    }

    ImFont* get_secondary_font(int size) const {
        if (m_secondary_font != nullptr) {
            if (ImFont* font = m_secondary_font->get(size); font != nullptr) {
                return font;
            }
        }

        return ImGui::GetCurrentContext() == nullptr ? nullptr : ImGui::GetFont();
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

    /// application nodes should normally be owned below this retained root.
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
    ui::Font* m_primary_font = nullptr;
    ui::Font* m_secondary_font = nullptr;
    bool m_done = false;
    bool m_ready = false;
};
