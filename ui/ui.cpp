#include "ui.hpp"

#include "imgui/context-scope.hpp"
#include "layout/layer-container.hpp"
#include "style/theme.hpp"

#include <algorithm>
#include <utility>

UI::UI(ui::Runtime& runtime, std::unique_ptr<ui::Backend> backend)
    : m_runtime(runtime), m_backend(std::move(backend)), m_profiler(runtime.performance_directory()) {
    initialize();
}

UI::~UI() {
    if (m_context == nullptr) {
        return;
    }

    const ui::ImGuiContextScope scope(m_context);

    m_effects.shutdown();
    m_runtime.release_context(m_context);
    if (m_backend != nullptr) m_backend->shutdown_imgui();
    ImGui::DestroyContext(m_context);
}

void UI::set_theme(ui::Theme theme) {
    m_runtime.set_theme(std::move(theme));
    if (!m_ready || m_context == nullptr) {
        return;
    }

    const ui::ImGuiContextScope scope(m_context);
    apply_theme_colors();
    if (m_root != nullptr) {
        m_root->apply_theme(m_runtime.theme());
    }
}

ImFont* UI::get_font(std::string_view id, int size) const {
    if (m_context == nullptr) {
        return nullptr;
    }

    const ui::ImGuiContextScope scope(m_context);
    if (ui::Font* font = m_runtime.fonts().find(id); font != nullptr) {
        if (ImFont* result = font->get(size); result != nullptr) {
            return result;
        }
    }

    return ImGui::GetFont();
}

void UI::initialize() {
    if (m_backend != nullptr) {
        if (!m_backend->initialize()) {
            return;
        }
        m_backend->make_current();
    }

    m_context = ImGui::CreateContext();

    if (m_context == nullptr) {
        return;
    }

    const ui::ImGuiContextScope scope(m_context);

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    configure_style(m_backend == nullptr ? 1.0F : m_backend->content_scale());

    if (m_backend != nullptr) m_backend->register_effects(m_effects);

    if (m_backend != nullptr && !m_backend->initialize_imgui()) {
        m_effects.shutdown();
        ImGui::DestroyContext(m_context);
        m_context = nullptr;
        return;
    }

    if (!m_effects.initialize()) {
        if (m_backend != nullptr) {
            m_backend->shutdown_imgui();
        }
        ImGui::DestroyContext(m_context);
        m_context = nullptr;
        return;
    }

    m_ready = true;

    m_root = std::make_unique<ui::LayerContainer>("ui-surface", ui::LayerMode::Window);
    m_root->set_input_router(&m_input_router);
    m_root->set_profiler(&m_profiler);
    m_profiler.set_root_node(m_root->identity());
}

void UI::begin_input_frame() {
    if (!m_ready) {
        return;
    }

    const ui::ImGuiContextScope scope(m_context);
    m_input_router.begin_frame();
}

void UI::configure_style(float main_scale) {
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    const ui::Theme& theme = m_runtime.theme();

    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.PopupRounding = 6.0f;
    style.TabRounding = 4.0f;
    style.WindowPadding = ImVec2{0.0f, 0.0f};
    style.CellPadding = ImVec2{0.0f, 0.0f};

    style.FramePadding = {12.0F, 8.0F};
    style.FrameRounding = std::max(0.0F, theme.control_rounding);
    style.FrameBorderSize = 0.0F;
    style.GrabMinSize = std::max(1.0F, theme.control_thumb_size);
    style.GrabRounding = std::max(0.0F, theme.control_rounding);
    style.ItemSpacing = {10.0F, 10.0F};
    style.ItemInnerSpacing = {8.0F, 6.0F};

    style.CircleTessellationMaxError = 0.10f;
    style.AntiAliasedLinesUseTex = false;

    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    apply_theme_colors();
}

void UI::apply_theme_colors() {
    const ui::Theme& theme = m_runtime.theme();
    ImVec4* colors = ImGui::GetStyle().Colors;

    colors[ImGuiCol_WindowBg] = theme.background_color;
    colors[ImGuiCol_ChildBg] = theme.background_secondary_color;
    colors[ImGuiCol_Border] = theme.control_border_color;
    colors[ImGuiCol_Separator] = theme.header_border_color;
    colors[ImGuiCol_Text] = theme.text_color;
    colors[ImGuiCol_TextDisabled] = theme.text_secondary_color;
    colors[ImGuiCol_Button] = theme.background_secondary_color;
    colors[ImGuiCol_ButtonHovered] = theme.background_tertiary_color;
    colors[ImGuiCol_ButtonActive] = theme.button_active_color;
    colors[ImGuiCol_Header] = theme.header_background_color;
    colors[ImGuiCol_HeaderHovered] = theme.background_tertiary_color;
    colors[ImGuiCol_HeaderActive] = theme.button_active_color;
    colors[ImGuiCol_Tab] = theme.background_tertiary_color;
    colors[ImGuiCol_TabHovered] = theme.accent_hover_color;
    colors[ImGuiCol_TabSelected] = theme.accent_color;
    colors[ImGuiCol_TabSelectedOverline] = theme.accent_color;
    colors[ImGuiCol_TabDimmed] = theme.background_tertiary_color;
    colors[ImGuiCol_TabDimmedSelected] = theme.accent_color;
    colors[ImGuiCol_TabDimmedSelectedOverline] = theme.accent_color;
    colors[ImGuiCol_FrameBg] = theme.control_background_color;
    colors[ImGuiCol_FrameBgHovered] = theme.control_hover_color;
    colors[ImGuiCol_FrameBgActive] = theme.control_active_color;
    colors[ImGuiCol_PopupBg] = theme.control_background_color;
    colors[ImGuiCol_ScrollbarBg] = theme.scrollbar_background_color;
    colors[ImGuiCol_CheckboxSelectedBg] = theme.control_background_color;
    colors[ImGuiCol_TitleBg] = theme.background_secondary_color;
    colors[ImGuiCol_TitleBgActive] = theme.background_secondary_color;
    colors[ImGuiCol_CheckMark] = theme.control_mark_color;
    colors[ImGuiCol_SliderGrab] = theme.control_mark_color;
    colors[ImGuiCol_SliderGrabActive] = theme.accent_hover_color;
}

bool UI::dispatch(ui::UiEvent& event) {
    return m_ready && input_router().dispatch(event);
}

void UI::begin_frame() {
    if (!m_ready) {
        return;
    }

    m_previous_context = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(m_context);
    if (m_backend != nullptr) m_backend->make_current();

    m_profiler.begin_frame();

    m_effects.begin_frame();

    if (m_backend != nullptr) m_backend->begin_frame(m_runtime.theme().background_color);
    ImGui::NewFrame();
}

void UI::end_frame() {
    if (!m_ready) {
        return;
    }

    if (m_backend != nullptr) m_backend->set_mouse_cursor(ImGui::GetMouseCursor());
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    if (m_profiler.enabled()) {
        const ui::InputRouterStats input_stats = m_input_router.stats();
        m_profiler.record_frame_metrics(input_stats.entry_count, input_stats.entry_checks);
    }
    if (m_backend != nullptr) {
        UI_PROFILE_SCOPE(&m_profiler, "UI::render");
        m_backend->render(draw_data);
    }
    m_profiler.end_frame();

    ImGui::SetCurrentContext(m_previous_context);
    m_previous_context = nullptr;
}
