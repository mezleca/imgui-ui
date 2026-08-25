#pragma once

#include "backend.hpp"
#include "style/theme.hpp"
#include "diagnostics/profiler.hpp"
#include "runtime.hpp"
#include "imgui/input-bridge.hpp"
#include "resources/assets.hpp"

#include <imgui.h>
#include <memory>

class UI;
class IconTexture;

namespace ui {
    class Node;
} // namespace ui

/// assets and theme remain owned by Runtime; imgui context, root and router are surface-local.
class UI {
public:
    UI(ui::Runtime& runtime, const ui::Config& config);
    UI(ui::Runtime& runtime, std::unique_ptr<ui::Backend> backend);
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

    ui::Font& get_font(ui::FontType type) {
        return m_runtime.font(type);
    }

    const ui::Font& get_font(ui::FontType type) const {
        return m_runtime.font(type);
    }

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

    ui::ImGuiInputBridge& input() {
        return m_imgui_input;
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

    const ui::Theme& theme() const {
        return m_runtime.theme();
    }

    ui::Runtime& runtime() {
        return m_runtime;
    }

    /// application nodes should normally be owned below this retained root.
    ui::Node& root() {
        return *m_container;
    }

    ui::Backend& backend() {
        return *m_backend;
    }

    ImGuiContext* imgui_context() {
        return m_context;
    }

    IconTexture* get_texture(std::string_view id);

    void set_frame_style(ImVec2 padding, float rounding, float border_thickness);
    void set_grab_style(float minimum_size, float rounding);
    void set_item_spacing(ImVec2 spacing, ImVec2 inner_spacing);

private:
    void initialize();
    void configure_style(float main_scale);
    void apply_theme_colors();

    ui::Runtime& m_runtime;
    ImGuiContext* m_context = nullptr;
    ImGuiContext* m_previous_context = nullptr;
    std::unique_ptr<ui::Backend> m_backend;
    std::unique_ptr<ui::Node> m_container;
    ui::InputRouter m_input_router;
    ui::ImGuiInputBridge m_imgui_input;
    ui::Profiler m_profiler;
    ui::Font* m_primary_font = nullptr;
    ui::Font* m_secondary_font = nullptr;
    bool m_done = false;
    bool m_ready = false;
};
