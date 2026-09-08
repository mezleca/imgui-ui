#include "ui.hpp"

#include "imgui/context-scope.hpp"
#include "diagnostics/debugger.hpp"
#include "layout/layer-container.hpp"
#include "layout/resizable-container.hpp"
#include "layout/stack-container.hpp"
#include "style/theme.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace ui {

    class SurfaceContent final : public ui::ResizableContainer {
    public:
        SurfaceContent(ui::Node& surface_root, const ui::Theme& theme)
            : ui::ResizableContainer("content"), m_surface_root(surface_root) {
            set_type_name("SurfaceContent");
            set_size({ui::grow(), ui::grow()});
            apply_theme_defaults(theme);
        }

        void update(float dt) override {
            if (m_forwarding) {
                ui::Node::update(dt);
                return;
            }

            // enter the internal tree so the proxy updates its application children on re-entry.
            m_forwarding = true;
            m_surface_root.update(dt);
            m_forwarding = false;
        }

        void draw() override {
            if (m_forwarding) {
                ui::StyledNode::draw();
                return;
            }

            m_forwarding = true;
            m_surface_root.draw();
            m_forwarding = false;
        }

        void apply_theme_defaults(const ui::Theme& theme) override {
            configure_all_styles([&theme](ui::Style& style) {
                style.background_color(theme.background_color).border_color(theme.controls.border_color).border(ui::BORDER_NONE);
            });
        }

    private:
        ui::Node& m_surface_root;
        bool m_forwarding = false;
    };

    UI::UI(ui::Runtime& runtime, ui::UIConfig config)
        : m_runtime(runtime), m_backend(std::move(config.backend)), m_profiler(runtime.performance_directory()) {
        initialize();

        if (m_ready && config.enable_debugger) {
            m_debugger = &m_surface_layout->add<ui::Debugger>(*this);
        }
    }

    UI::~UI() {
        ImGuiContext* previous_context = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(m_context);

        m_root.reset();
        m_surface_layout = nullptr;
        m_content_root = nullptr;
        m_debugger = nullptr;

        m_effects.shutdown();
        m_runtime.release_context(m_context);
        m_backend->shutdown_imgui();
        ImGui::DestroyContext(m_context);
        ImGui::SetCurrentContext(previous_context == m_context ? nullptr : previous_context);
        m_context = nullptr;
    }

    void UI::set_theme(ui::Theme theme) {
        m_runtime.set_theme(theme);

        if (!m_ready || m_context == nullptr) {
            return;
        }

        const ui::ImGuiContextScope scope(m_context);

        apply_theme_metrics();
        apply_theme_colors();

        if (m_root != nullptr) {
            m_root->apply_theme(m_runtime.theme());
        }
    }

    ImFont* UI::resolve_font(ui::Font* font, int size) const {
        if (font != nullptr) {
            if (ImFont* result = font->get(size); result != nullptr) {
                return result;
            }
        }

        return ImGui::GetCurrentContext() == nullptr ? nullptr : ImGui::GetFont();
    }

    ImFont* UI::get_font(std::string_view id, int size) const {
        if (m_context == nullptr) {
            return nullptr;
        }

        const ui::ImGuiContextScope scope(m_context);
        return resolve_font(m_runtime.fonts().find(id), size);
    }

    void UI::initialize() {
        if (m_backend == nullptr) {
            throw std::runtime_error("m_backend is nullptr");
        }

        if (!m_backend->initialize()) {
            throw std::runtime_error("failed to initialize backend");
        }

        m_context = ImGui::CreateContext();
        const ui::ImGuiContextScope scope(m_context);

        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;

        configure_style(m_backend->content_scale());
        m_backend->register_effects(m_effects);

        if (!m_backend->initialize_imgui()) {
            throw std::runtime_error("failed to initialize imgui");
        }

        if (!m_effects.initialize()) {
            throw std::runtime_error("failed to initialize effects");
        }

        m_ready = true;

        m_root = std::make_unique<ui::LayerContainer>("ui-surface", ui::LayerMode::Window);
        m_root->set_input_router(&m_input_router);
        m_root->set_profiler(&m_profiler);

        auto& surface_layout = m_root->add<ui::StackContainer>("ui-root", ui::StackDirection::Horizontal);
        surface_layout.set_size({ui::grow(), ui::grow()});
        m_surface_layout = &surface_layout;

        auto& content = surface_layout.add<SurfaceContent>(*m_root, m_runtime.theme());
        m_content_root = &content;
        m_profiler.set_root_node(content.identity());
    }

    void UI::configure_style(float main_scale) {
        ImGui::StyleColorsDark();

        m_content_scale = main_scale;
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(main_scale);
        style.FontScaleDpi = main_scale;

        apply_theme_metrics();
        apply_theme_colors();
    }

    void UI::apply_theme_metrics() {
        ImGuiStyle& style = ImGui::GetStyle();
        const ui::Theme& theme = m_runtime.theme();
        const float scale = m_content_scale;
        const auto scaled = [scale](float value) { return std::max(0.0F, value) * scale; };
        const auto scaled_size = [scale](ImVec2 value) {
            return ImVec2{std::max(0.0F, value.x) * scale, std::max(0.0F, value.y) * scale};
        };

        style.WindowRounding = scaled(theme.metrics.window_rounding);
        style.ChildRounding = scaled(theme.metrics.child_rounding);
        style.PopupRounding = scaled(theme.metrics.popup_rounding);
        style.TabRounding = scaled(theme.metrics.tab_rounding);
        style.WindowPadding = scaled_size(theme.metrics.window_padding);
        style.CellPadding = scaled_size(theme.metrics.cell_padding);
        style.FramePadding = scaled_size(theme.metrics.frame_padding);
        style.FrameRounding = scaled(theme.controls.rounding);
        style.FrameBorderSize = scaled(theme.metrics.frame_border_size);
        style.GrabMinSize = std::max(1.0F, theme.controls.thumb_size) * scale;
        style.GrabRounding = scaled(theme.controls.rounding);
        style.ItemSpacing = scaled_size(theme.metrics.item_spacing);
        style.ItemInnerSpacing = scaled_size(theme.metrics.item_inner_spacing);
        style.CircleTessellationMaxError = std::max(0.0F, theme.metrics.circle_tessellation_max_error);
        style.AntiAliasedLinesUseTex = theme.metrics.anti_aliased_lines_use_tex;
    }

    void UI::apply_theme_colors() {
        const ui::Theme& theme = m_runtime.theme();
        ImVec4* colors = ImGui::GetStyle().Colors;

        colors[ImGuiCol_WindowBg] = theme.background_color;
        colors[ImGuiCol_ChildBg] = theme.background_secondary_color;
        colors[ImGuiCol_Border] = theme.controls.border_color;
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
        colors[ImGuiCol_FrameBg] = theme.controls.background_color;
        colors[ImGuiCol_FrameBgHovered] = theme.controls.hover_color;
        colors[ImGuiCol_FrameBgActive] = theme.controls.active_color;
        colors[ImGuiCol_PopupBg] = theme.controls.background_color;
        colors[ImGuiCol_ScrollbarBg] = theme.scrollbar_background_color;
        colors[ImGuiCol_CheckboxSelectedBg] = theme.controls.background_color;
        colors[ImGuiCol_TitleBg] = theme.background_secondary_color;
        colors[ImGuiCol_TitleBgActive] = theme.background_secondary_color;
        colors[ImGuiCol_CheckMark] = theme.controls.mark_color;
        colors[ImGuiCol_SliderGrab] = theme.controls.mark_color;
        colors[ImGuiCol_SliderGrabActive] = theme.accent_hover_color;
    }

    bool UI::dispatch(ui::UiEvent& event) {
        if (!m_ready) {
            return false;
        }

        const ui::ImGuiContextScope scope(m_context);
        if (m_debugger != nullptr && m_debugger->handle_input(event)) {
            return true;
        }

        return input_router().dispatch(event);
    }

    bool UI::debugger_blocks_pointer_input() const {
        return m_debugger != nullptr && m_debugger->blocks_pointer_input();
    }

    void UI::begin_frame() {
        if (!m_ready) {
            return;
        }

        m_previous_context = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(m_context);

        m_input_router.begin_frame();
        m_profiler.begin_frame();

        m_effects.begin_frame();
        m_backend->begin_frame(m_runtime.theme().background_color);

        ImGui::NewFrame();

        if (m_debugger != nullptr) {
            m_debugger->update();
        }
    }

    void UI::update(float dt) {
        if (m_ready) {
            m_content_root->update(dt);
        }
    }

    void UI::draw() {
        if (m_ready) {
            m_content_root->draw();
        }
    }

    void UI::end_frame() {
        if (!m_ready) {
            return;
        }

        m_backend->set_mouse_cursor(ImGui::GetMouseCursor());

        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();

        if (m_profiler.enabled()) {
            const ui::InputRouterStats input_stats = m_input_router.stats();
            m_profiler.record_frame_metrics(input_stats.entry_count, input_stats.entry_checks);
        }

        UI_PROFILE_SCOPE(&m_profiler, "UI::render");
        m_backend->render(draw_data);

        m_profiler.end_frame();
        ImGui::SetCurrentContext(m_previous_context);
        m_previous_context = nullptr;
    }

} // namespace ui
