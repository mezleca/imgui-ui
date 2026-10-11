#include "demo.hpp"

#include <imgui-ui/backends/opengl/texture-loader.hpp>
#include <imgui-ui/imgui/effects/effects.hpp>
#include <imgui-ui/imgui/effects/opengl.hpp>
#include <imgui-ui/layout/layer-container.hpp>
#include <imgui-ui/layout/resizable-container.hpp>
#include <imgui-ui/layout/virtual-layout.hpp>
#include <imgui-ui/resources/texture-registry.hpp>
#include <imgui-ui/style/style.hpp>
#include <imgui-ui/style/styled-node.hpp>
#include <imgui-ui/surface.hpp>
#include <imgui-ui/widgets/button.hpp>
#include <imgui-ui/widgets/checkbox.hpp>
#include <imgui-ui/widgets/color-picker.hpp>
#include <imgui-ui/widgets/context-menu.hpp>
#include <imgui-ui/widgets/dropdown.hpp>
#include <imgui-ui/widgets/file-dialog.hpp>
#include <imgui-ui/widgets/image.hpp>
#include <imgui-ui/widgets/number-input.hpp>
#include <imgui-ui/widgets/text-input.hpp>
#include <imgui-ui/widgets/text.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <memory>
#include <random>
#include <string_view>
#include <utility>
#include <unordered_map>
#include <vector>

using namespace ui;

static constexpr std::string_view DEMO_INLINE_ICON_SVG = R"(
    <svg width="24" height="24" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
        <circle cx="12" cy="12" r="8" stroke="white" stroke-width="2"/>
        <path d="M12 8V12L15 14" stroke="white" stroke-width="2" stroke-linecap="round"/>
    </svg>)";

enum class DemoThemeVariant {
    Default,
    Pastel,
    Material,
};

enum class DemoPanelTone {
    Base,
    Secondary,
    Tertiary,
};

struct DemoChromaticCommand {
    Rect rect;
};

struct DemoChromaticState {
    OpenGlFullscreenEffect effect;
};

// adapted from https://ziyadbarakat.wordpress.com/tutorials/graphics/chromatic-aberration-shader/.
static constexpr const char* DEMO_CHROMATIC_FRAGMENT_SHADER = R"(#version 330 core
uniform sampler2D defaultTexture;
uniform vec4 bounds;
uniform float redOffset;
uniform float greenOffset;
uniform float blueOffset;
out vec4 outColor;

void main() {
vec2 uv = (gl_FragCoord.xy - bounds.xy) / (bounds.zw - bounds.xy);
vec2 offset = vec2(1.0 / max(bounds.z - bounds.x, 1.0), 0.0);
float redValue = texture(defaultTexture, clamp(uv + offset * redOffset, 0.0, 1.0)).r;
float greenValue = texture(defaultTexture, clamp(uv + offset * greenOffset, 0.0, 1.0)).g;
float blueValue = texture(defaultTexture, clamp(uv + offset * blueOffset, 0.0, 1.0)).b;
outColor = vec4(redValue, greenValue, blueValue, texture(defaultTexture, uv).a);
})";

static bool initialize_demo_chromatic(void* data) {
    return static_cast<DemoChromaticState*>(data)->effect.initialize(DEMO_CHROMATIC_FRAGMENT_SHADER);
}

static void shutdown_demo_chromatic(void* data) {
    static_cast<DemoChromaticState*>(data)->effect.shutdown();
}

static void render_demo_chromatic(void* data, const ImDrawList*, const ImDrawCmd* draw_command, const void* payload) {
    auto* state = static_cast<DemoChromaticState*>(data);
    const auto* chromatic = static_cast<const DemoChromaticCommand*>(payload);
    if (chromatic == nullptr || !state->effect.begin(*draw_command, chromatic->rect) || !state->effect.capture()) {
        return;
    }

    const Rect bounds = state->effect.framebuffer_bounds();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, state->effect.captured_texture());
    glUniform1i(state->effect.uniform("defaultTexture"), 0);
    glUniform4f(state->effect.uniform("bounds"), bounds.min.x, bounds.min.y, bounds.max.x, bounds.max.y);
    glUniform1f(state->effect.uniform("redOffset"), 2.0F);
    glUniform1f(state->effect.uniform("greenOffset"), 0.5F);
    glUniform1f(state->effect.uniform("blueOffset"), -2.0F);
    OpenGlFullscreenEffect::draw();
}

static DemoChromaticState demo_chromatic_state;

// demo subclasses read this variant when the single demo surface reapplies its theme.
static DemoThemeVariant s_demo_theme_variant = DemoThemeVariant::Default;

static DemoThemeVariant demo_theme_variant(std::string_view variant) {
    if (variant == "pastel") return DemoThemeVariant::Pastel;
    if (variant == "material") return DemoThemeVariant::Material;
    return DemoThemeVariant::Default;
}

static Theme make_demo_theme(std::string_view variant) {
    Theme theme{};

    if (variant == "pastel") {
        theme.controls.rounding = 10.0F;
        theme.controls.border_thickness = 1.0F;
        theme.controls.thumb_size = 14.0F;
        theme.accent_color = rgb(226, 180, 189);
        theme.accent_hover_color = rgb(214, 153, 165);
        theme.background_color = rgb(255, 245, 245);
        theme.background_secondary_color = rgb(255, 250, 250);
        theme.background_tertiary_color = rgb(247, 214, 208);
        theme.scrollbar.background_color = rgba(226.0F / 255.0F, 180.0F / 255.0F, 189.0F / 255.0F, 0.55F);
        theme.header_background_color = rgb(247, 214, 208);
        theme.text_color = rgb(74, 74, 74);
        theme.text_secondary_color = rgb(106, 87, 90);
        theme.border_color = rgb(226, 180, 189);
        theme.header_border_color = rgba(214.0F / 255.0F, 153.0F / 255.0F, 165.0F / 255.0F, 0.55F);
        theme.button_active_color = rgba(226.0F / 255.0F, 180.0F / 255.0F, 189.0F / 255.0F, 0.35F);
        theme.controls.background_color = rgb(255, 245, 245);
        theme.controls.hover_color = rgb(247, 214, 208);
        theme.controls.active_color = rgba(226.0F / 255.0F, 180.0F / 255.0F, 189.0F / 255.0F, 0.35F);
        theme.controls.border_color = rgb(226, 180, 189);
        theme.metrics.popup_rounding = 10.0F;
        theme.metrics.tab_rounding = 8.0F;
        theme.metrics.item_spacing = {12.0F, 10.0F};
    } else if (variant == "material") {
        theme.controls.rounding = 12.0F;
        theme.controls.border_thickness = 1.0F;
        theme.controls.thumb_size = 16.0F;
        theme.accent_color = rgb(0.82F, 0.74F, 1.0F);
        theme.accent_hover_color = rgb(0.91F, 0.87F, 0.97F);
        theme.background_color = rgb(0.078F, 0.071F, 0.094F);
        theme.background_secondary_color = rgb(0.129F, 0.122F, 0.149F);
        theme.background_tertiary_color = rgb(0.059F, 0.051F, 0.075F);
        theme.scrollbar.background_color = rgb(0.059F, 0.051F, 0.075F);
        theme.header_background_color = rgb(0.169F, 0.161F, 0.188F);
        theme.text_color = rgb(0.902F, 0.878F, 0.914F);
        theme.text_secondary_color = rgb(0.792F, 0.769F, 0.816F);
        theme.border_color = rgb(0.286F, 0.271F, 0.31F);
        theme.header_border_color = rgba(0.475F, 0.455F, 0.494F, 0.35F);
        theme.button_active_color = rgba(0.82F, 0.74F, 1.0F, 0.28F);
        theme.controls.background_color = rgb(0.129F, 0.122F, 0.149F);
        theme.controls.hover_color = rgb(0.212F, 0.196F, 0.239F);
        theme.controls.active_color = rgba(0.82F, 0.74F, 1.0F, 0.24F);
        theme.controls.border_color = rgb(0.475F, 0.455F, 0.494F);
        theme.metrics.window_rounding = 4.0F;
        theme.metrics.child_rounding = 4.0F;
        theme.metrics.popup_rounding = 12.0F;
        theme.metrics.tab_rounding = 8.0F;
        theme.metrics.frame_padding = {14.0F, 10.0F};
        theme.metrics.item_spacing = {12.0F, 12.0F};
        theme.metrics.item_inner_spacing = {8.0F, 8.0F};
    }

    theme.controls.mark_color = theme.accent_color;
    return theme;
}

class DemoTextInput final : public TextInputWidget {
public:
    using TextInputWidget::TextInputWidget;

protected:
    void apply_theme_defaults(const Theme& theme) override {
        TextInputWidget::apply_theme_defaults(theme);

        switch (s_demo_theme_variant) {
            case DemoThemeVariant::Default:
                return;
            case DemoThemeVariant::Pastel:
                set_spacing(12.0F);
                configure_all_styles([](Style& style) { style.padding({14.0F, 12.0F}).border_radius(6.0F); });
                return;
            case DemoThemeVariant::Material:
                set_spacing(12.0F);
                configure_all_styles([](Style& style) { style.padding({16.0F, 12.0F}).border_radius(12.0F); });
                return;
        }
    }
};

class DemoDropdown final : public DropdownWidget {
public:
    using DropdownWidget::DropdownWidget;

protected:
    void apply_theme_defaults(const Theme& theme) override {
        DropdownWidget::apply_theme_defaults(theme);

        switch (s_demo_theme_variant) {
            case DemoThemeVariant::Default:
                return;
            case DemoThemeVariant::Pastel:
                trigger().configure_all_styles([](Style& style) { style.padding({12.0F, 7.0F}).border_radius(10.0F); });
                return;
            case DemoThemeVariant::Material:
                trigger().configure_all_styles([](Style& style) { style.padding({16.0F, 8.0F}).border_radius(12.0F); });
                return;
        }
    }
};

class DemoCheckbox final : public CheckboxWidget {
public:
    using CheckboxWidget::CheckboxWidget;

protected:
    void apply_theme_defaults(const Theme& theme) override {
        CheckboxWidget::apply_theme_defaults(theme);

        switch (s_demo_theme_variant) {
            case DemoThemeVariant::Default:
                return;
            case DemoThemeVariant::Pastel:
                set_box_size(22.0F);
                frame().configure_all_styles([](Style& style) { style.border_radius(6.0F); });
                fill().configure_all_styles([](Style& style) { style.border_radius(6.0F); });
                return;
            case DemoThemeVariant::Material:
                set_box_size(24.0F);
                frame().configure_all_styles([](Style& style) { style.border_radius(5.0F); });
                fill().configure_all_styles([](Style& style) { style.border_radius(5.0F); });
                return;
        }
    }
};

class DemoNumberInput final : public NumberInputWidget {
public:
    using NumberInputWidget::NumberInputWidget;

protected:
    void apply_theme_defaults(const Theme& theme) override {
        NumberInputWidget::apply_theme_defaults(theme);

        switch (s_demo_theme_variant) {
            case DemoThemeVariant::Default:
                return;
            case DemoThemeVariant::Pastel:
                configure_all_styles([](Style& style) { style.padding({12.0F, 7.0F}).border_radius(10.0F); });
                return;
            case DemoThemeVariant::Material:
                configure_all_styles([](Style& style) { style.padding({16.0F, 8.0F}).border_radius(12.0F); });
                return;
        }
    }
};

class DemoColorPicker final : public ColorPickerWidget {
public:
    using ColorPickerWidget::ColorPickerWidget;

protected:
    void apply_theme_defaults(const Theme& theme) override {
        ColorPickerWidget::apply_theme_defaults(theme);

        switch (s_demo_theme_variant) {
            case DemoThemeVariant::Default:
                return;
            case DemoThemeVariant::Pastel:
                preview().configure_all_styles([](Style& style) { style.border_radius(6.0F); });
                return;
            case DemoThemeVariant::Material:
                preview().configure_all_styles([](Style& style) { style.border_radius(12.0F); });
                return;
        }
    }
};

void configure_demo_runtime(RuntimeConfig& config) {
    config.theme = make_demo_theme("default");

#ifdef IMGUI_UI_ASSETS_DIR
    config.texture_loader = std::make_unique<OpenGLTextureLoader>();
#endif
}

class DemoPanel : public Container {
public:
    explicit DemoPanel(
        std::string id, DemoPanelTone tone = DemoPanelTone::Base, ImVec2 padding = {14.0F, 14.0F}, uint8_t border = BORDER_NONE,
        bool accent_border = false, bool shadow = false
    )
        : Container(std::move(id)), m_tone(tone), m_padding(padding), m_border(border), m_accent_border(accent_border),
          m_shadow(shadow) {}

protected:
    void apply_theme_defaults(const Theme& theme) override {
        Container::apply_theme_defaults(theme);
        const Color* background = &theme.background_color;
        if (m_tone == DemoPanelTone::Secondary) background = &theme.background_secondary_color;
        if (m_tone == DemoPanelTone::Tertiary) background = &theme.background_tertiary_color;

        configure_all_styles([&](Style& style) {
            style.padding(m_padding)
                .background_color(*background)
                .border(m_border)
                .border_color(m_accent_border ? theme.accent_color : theme.border_color)
                .border_radius(4.0F)
                .box_shadow(m_shadow ? BoxShadow{
                                           .offset = {0.0F, 8.0F},
                                           .blur = 18.0F,
                                           .spread = 2.0F,
                                           .color = rgba(0.0F, 0.0F, 0.0F, 0.45F),
                                       }
                                 : BoxShadow{});
        });
    }

private:
    DemoPanelTone m_tone;
    ImVec2 m_padding;
    uint8_t m_border;
    bool m_accent_border;
    bool m_shadow;
};

class DemoChromaticPanel final : public DemoPanel {
public:
    explicit DemoChromaticPanel(EffectPass effect) : DemoPanel("demo-custom-effect", DemoPanelTone::Secondary), m_effect(effect) {
        set_size({grow(), px(128.0F)});
        set_spacing(6.0F);
    }

protected:
    void apply_theme_defaults(const Theme& theme) override {
        DemoPanel::apply_theme_defaults(theme);
        configure_all_styles([](Style& style) {
            style.background_color(rgb(0.0F, 0.0F, 0.0F)).border(BORDER_ALL).border_color(rgb(0.5F, 0.5F, 0.5F));
        });
    }

    void on_draw_end() override {
        const Rect rect = Rect::from_position_size(ImGui::GetWindowPos(), ImGui::GetWindowSize());

        // on_draw_end runs after this panel's children draw. submitting the effect here captures their pixels too,
        // rather than capturing only the panel background before the children have been rendered.
        m_effect.submit(*ImGui::GetWindowDrawList(), DemoChromaticCommand{rect});
        DemoPanel::on_draw_end();
    }

private:
    EffectPass m_effect;
};

class DemoGradientPanel final : public DemoPanel {
public:
    DemoGradientPanel(GradientType type, std::string id) : DemoPanel(std::move(id), DemoPanelTone::Secondary), m_type(type) {
        set_size({grow(), px(128.0F)});
        set_spacing(6.0F);
    }

protected:
    void apply_theme_defaults(const Theme& theme) override {
        DemoPanel::apply_theme_defaults(theme);
        const Color background = gradient(m_type, {{0.0F, theme.accent_hover_color}, {1.0F, theme.background_tertiary_color}});
        configure_all_styles([&](Style& style) {
            style.background_color(background);
            if (m_type == GradientType::Radial) {
                style.box_shadow({
                    .offset = {0.0F, 8.0F},
                    .blur = 18.0F,
                    .spread = 2.0F,
                    .color = gradient(
                        GradientType::Radial, {{0.0F, rgba(0.26F, 0.59F, 0.98F, 0.55F)}, {1.0F, rgba(0.26F, 0.59F, 0.98F, 0.0F)}}
                    ),
                });
            }
        });
    }

private:
    GradientType m_type;
};

class DemoFloatingWindow final : public LayerContainer {
public:
    DemoFloatingWindow() : LayerContainer("demo-floating-window", LayerMode::Window) {
        // set_layout replaces size, placement and in_flow together. omitted fields receive LayoutConfig defaults,
        // including grow/grow sizing. calling set_size before set_layout would therefore lose that size request.
        // anchor selects a point in the parent's content box. origin selects the point on this window to align with it.
        // matching TopRight points puts the window to the left of that corner. offset {-24, 24} moves it left and down.
        // in_flow=false positions it independently of siblings, without reserving any space in the parent's stack.
        set_layout({
            .size = {px(320.0F), px(180.0F)},
            .placement = {.anchor = Anchor::TopRight, .origin = Anchor::TopRight, .offset = {-24.0F, 24.0F}},
            .in_flow = false,
        });
        set_spacing(12.0F);
        on_event([](UiEvent& event) {
            if (ui::contains(EventMask::Pointer, event_mask(event.type))) event.stop_propagation();
        });

        add<TextWidget>("floating window");
        add<TextWidget>("drag the empty area to move\ndrag a corner to resize");
    }

protected:
    ImGuiWindowFlags window_flags() const override {
        // LayerMode::Window creates a separate imgui window. the default flags prevent moving, resizing and raising it.
        // clearing these flags enables those interactions. the layer then submits layout position and size with
        // ImGuiCond_FirstUseEver, so the layout initializes the window without overwriting the user's later changes.
        return LayerContainer::window_flags() &
               ~(ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);
    }

    void apply_theme_defaults(const Theme& theme) override {
        LayerContainer::apply_theme_defaults(theme);
        set_font(surface().get_primary_font(), 16.0F);
        configure_all_styles([&theme](Style& style) {
            style.background_color(theme.background_secondary_color)
                .padding({14.0F, 14.0F})
                .border(BORDER_ALL)
                .border_color(theme.accent_color)
                .border_radius(8.0F);
        });
    }
};

class InputBlocker final : public DemoPanel {
public:
    InputBlocker() : DemoPanel("InputBlocker") {
        set_spacing(10.0F);

        // fit/fit sizes this panel from its children, spacing, padding and borders, rather than filling the backdrop.
        // Center/Center aligns the panel's center with the backdrop's content center, regardless of the measured size.
        // in_flow=false excludes the panel from the parent's stack and fit measurement. it remains visible and interactive.
        set_layout({
            .size = {fit(), fit()},
            .placement = {.anchor = Anchor::Center, .origin = Anchor::Center},
            .in_flow = false,
        });
    }

    void apply_theme_defaults(const Theme& theme) override {
        DemoPanel::apply_theme_defaults(theme);

        configure_all_styles([&theme](Style& style) {
            style.background_color(rgba(0, 0, 0, 0));
            style.blur(11);
            style.border(BORDER_ALL);
        });
    }
};

class DemoAccentButton final : public ButtonWidget {
public:
    DemoAccentButton(std::string text, LayoutSize size) : ButtonWidget(std::move(text), size) {}

protected:
    void apply_theme_defaults(const Theme& theme) override {
        ButtonWidget::apply_theme_defaults(theme);

        configure_all_styles([&theme](Style& style) {
            style.background_color(theme.accent_color).border_color(theme.accent_hover_color);
        });

        style(StyleType::HOVER).background_color(theme.accent_hover_color).border_color(theme.accent_hover_color);
        style(StyleType::ACTIVE).background_color(theme.accent_color).border_color(theme.accent_color);
    }
};

class DemoAnimatedText final : public TextWidget {
public:
    explicit DemoAnimatedText(std::string text) : TextWidget(std::move(text)) {
        set_input_mode(InputMode::Target);
    }

protected:
    void apply_theme_defaults(const Theme& theme) override {
        TextWidget::apply_theme_defaults(theme);
        configure_all_styles([](Style& style) { style.padding({}).background_color(rgba(0, 0, 0, 0)); });
    }

private:
    void input_state_changed() override {
        StyledNode::input_state_changed();

        const bool hovered = input_state().hovered;
        if (hovered == m_hovered) {
            return;
        }

        m_hovered = hovered;
        if (hovered) {
            // to animates a property toward an absolute value. calls before the next then run in parallel.
            // then(0.1F) starts the next phase after the longest animation in this phase finishes, plus a 0.1 second pause.
            // padding-x and background color therefore change together, followed by rotation in the final phase.
            animate()
                .to(StyleAnimationProperty::PaddingY, 10.0F, {0.2F, easing::out_quad})
                .then(0.1F)
                .to(StyleAnimationProperty::PaddingX, 20.0F, {0.24F, easing::out_cubic})
                .to(StyleAnimationProperty::BackgroundColor, surface().theme().accent_color, {0.24F, easing::out_cubic})
                .then(0.1F)
                .to(StyleAnimationProperty::Rotation, 180.0F, {0.25F, easing::out_cubic});
        } else {
            animate().release_all({0.15F, easing::linear});
        }
    }
    bool m_hovered = false;
};

static void apply_border_style(Node& node, BorderStyle style);

class DemoProgressWidget final : public DrawListWidget {
public:
    DemoProgressWidget() : DrawListWidget("demo-progress") {
        set_size({grow(), px(68.0F)});
    }

protected:
    void apply_theme_defaults(const Theme& theme) override {
        set_font(surface().get_primary_font(), 14.0F);
        m_fill_color = theme.accent_color;
        configure_all_styles([&theme](Style& style) {
            style.color(theme.text_color)
                .background_color(theme.background_tertiary_color)
                .border(BORDER_ALL)
                .border_color(theme.border_color)
                .border_radius(theme.controls.rounding)
                .border_thickness(theme.controls.border_thickness);
        });
    }

    void on_update(float) override {
        if (m_completed || layout().size().x <= 0.0F || animator().transitioning()) {
            return;
        }

        if (m_progress < 1.0F) {
            m_progress = std::min(1.0F, m_progress + 0.1F);
            const float fill_width = std::max(0.0F, layout().size().x - 12.0F) * m_progress;
            animator().animate().to(m_fill_width, fill_width, {m_duration_distribution(m_random), easing::out_cubic});
            return;
        }

        m_completed = true;
        animator()
            .animate()
            .to(m_message_offset, 30.0F, {0.32F, easing::out_quad})
            .to(m_message_opacity, 1.0F, {0.32F, easing::out_sine});
    }

private:
    void paint_content(const PaintContext& context) override {
        const Rect rect = context.rect;
        const ComputedStyle& style = context.style;
        const Rect box =
            Rect::from_position_size({rect.min.x + 6.0F, rect.min.y + 6.0F}, {std::max(0.0F, rect.size().x - 12.0F), 32.0F});
        const float fill_right = std::min(box.max.x, box.min.x + m_fill_width);

        context.painter.rect_filled(box, style.background_color().value, style.border_radius());
        if (fill_right > box.min.x) {
            context.painter.rect_filled({box.min, {fill_right, box.max.y}}, m_fill_color, style.border_radius());
        }
        context.painter.rect_outline(box, style.border_color().value, style.border_thickness(), style.border_radius());

        if (!m_completed) {
            return;
        }

        ImColor message_color = style.color().value.rgba();
        message_color.Value.w *= m_message_opacity;
        const ImVec2 message_size = ImGui::CalcTextSize("finished");
        context.painter.text(
            {box.min.x + ((box.size().x - message_size.x) * 0.5F), box.min.y + 8.0F + m_message_offset}, message_color, "finished"
        );
    }

    Color m_fill_color;
    float m_progress = 0.0F;
    float m_fill_width = 0.0F;
    float m_message_offset = 0.0F;
    float m_message_opacity = 0.0F;
    bool m_completed = false;
    std::mt19937 m_random{std::random_device{}()};
    std::uniform_real_distribution<float> m_duration_distribution{0.10F, 0.45F};
};

class DemoResizableNodes final : public ResizableContainer {
public:
    explicit DemoResizableNodes(std::string id) : ResizableContainer(std::move(id)) {}

protected:
    void apply_theme_defaults(const Theme& theme) override {
        ResizableContainer::apply_theme_defaults(theme);
        configure_all_styles([&theme](Style& style) {
            style.background_color(theme.background_tertiary_color)
                .border(BORDER_NONE)
                .border_radius(4.0F)
                .padding({20.0F, 20.0F})
                .cursor(ImGuiMouseCursor_ResizeNWSE);
        });
    }
};

class DemoTextListWidget final : public Container {
public:
    explicit DemoTextListWidget(std::vector<std::string> items);
    DemoTextListWidget& set_items(std::vector<std::string> items);

protected:
    void apply_theme_defaults(const Theme& theme) override {
        Container::apply_theme_defaults(theme);
        configure_all_styles([&theme](Style& style) {
            style.padding({8.0F, 8.0F}).background_color(theme.background_tertiary_color).border(BORDER_NONE).border_radius(4.0F);
        });
    }
};

class DemoScreen final : public Container {
public:
    DemoScreen();
    void setup(std::string backend);
    void setup_dynamic_nodes(Node& parent);
    int& blur();

private:
    ImageWidget& add_test_image(Texture* texture);
    void select_test_image(FileDialogWidget& dialog);
    ImageFit image_fit() const;
    void apply_image_fit();
    void on_update(float dt) override;

protected:
    void apply_theme_defaults(const Theme& theme) override {
        Container::apply_theme_defaults(theme);
        configure_all_styles([&theme](Style& style) {
            style.padding({12.0F, 12.0F}).background_color(theme.background_secondary_color);
        });
    }

private:
    ResizableContainer* m_dynamic_nodes = nullptr;
    TextWidget* m_dynamic_status = nullptr;
    TextWidget* m_fps = nullptr;
    float m_fps_update_elapsed = 0.0F;
    Container* m_test_images = nullptr;
    bool m_enabled = true;
    bool m_radio_selected = true;
    Color m_color = rgb(0.26F, 0.59F, 0.98F);
    int m_clicks = 0;
    DemoTextListWidget* m_text_list = nullptr;
    bool m_text_list_horizontal = false;
    int m_dynamic_count = 0;
    int m_next_dynamic_id = 0;
    float m_random_value = 0.0f;
    std::string m_name = "imgui-ui";
    std::string m_image_fit = "cover";
    std::string m_theme = "default";
    std::string m_border_style = "solid";
    int m_blur = 5;
};

DemoTextListWidget::DemoTextListWidget(std::vector<std::string> items) : Container("demo-text-list") {
    set_spacing(8.0F);
    set_size({fit(), fit()});
    set_items(std::move(items));
}

DemoTextListWidget& DemoTextListWidget::set_items(std::vector<std::string> items) {
    clear();
    for (std::string& item : items) {
        add<TextWidget>(std::move(item));
    }
    return *this;
}

DemoScreen::DemoScreen() : Container("demo", StackDirection::Vertical) {
    // set_size takes {width, height}. each axis can choose its own sizing rule:
    // - px(120) requests 120 pixels, independent of the available space.
    // - percent(50) requests half the available parent space, not half the remaining space after siblings.
    // - fit() measures the node's content. containers include flowing children, gaps, margins, padding and borders.
    // - grow() fills the available space. along a stack's direction, growing siblings share what remains after
    //   other children and gaps. grow(2) receives twice the share of grow(1). across the stack, grow fills that axis.
    // set_size changes only these two rules, preserving placement and in_flow. set_layout replaces all three.
    // containers default to BorderBox, so px/percent sizes include padding and borders. ContentBox adds them outside
    // the requested size. fit always includes them in its measured outer extent. margins belong to the parent layout.
    // here grow/grow keeps the demo sized to its parent as the window resizes, rather than expanding to fit all panels.
    // set_scrollable(vertical, horizontal) enables scrolling on the selected axes. this call enables vertical scrolling
    // only, so panels beyond the available height are reached by scrolling instead of enlarging the demo container.
    set_size({grow(), grow()});
    set_scrollable(true);
    set_spacing(16.0F);
}

void DemoScreen::setup(std::string backend) {
    Surface& ui = surface();
    auto& overview = add<DemoPanel>("overview");

    // ordinary nodes default to in_flow=true. a vertical container places them one below another and adds spacing
    // between them. a horizontal container places them side by side. only flowing children contribute to a parent's fit().
    // this panel fills the demo's width, but measures its height from its children. the next panel starts below that height.
    overview.set_size({grow(), fit()});
    overview.set_spacing(4.0F);
    overview.add<TextWidget>("imgui-ui example");

    overview.add<TextWidget>(std::format("backend: {}", backend));
    m_fps = &overview.add<TextWidget>("fps: 0.0");
    overview.add<TextWidget>("debugger: shift + d");
    auto& random_slider = overview.add<DemoNumberInput>(m_random_value);
    random_slider.set_label("random value");
    random_slider.set_maximum(10);

    auto& size_limits = add<DemoPanel>("size-limits");
    size_limits.set_size({grow(), fit()});
    size_limits.set_spacing(8.0F);
    size_limits.add<TextWidget>("size limits - resize the window to change the available width");
    auto& bounded_row = size_limits.add<Container>("bounded-row", StackDirection::Horizontal);
    bounded_row.set_size({grow(), px(100.0F)});
    bounded_row.set_spacing(8.0F);

    // set_size(minimum, maximum) requests grow on both axes within those limits. each limit accepts px() or percent().
    // this panel reserves at least 150 pixels of width and 60 of height, and grows to at most half the row's width
    // and 100 pixels of height. percentages refer to parent space, not the remainder left after the other panels.
    // if half the row becomes less than 150 pixels, the minimum wins. minima may overflow a parent that is too small.
    auto& bounded_grow = bounded_row.add<DemoPanel>("bounded-grow");
    bounded_grow.set_size({px(150.0F), px(60.0F)}, {percent(50.0F), px(100.0F)});
    bounded_grow.add<TextWidget>("150px - 50%");

    // ranges also work directly in LayoutSize, including widget constructors and set_layout's size field.
    // two values on an axis mean {minimum, maximum}, with grow as the rule. three mean {rule, minimum, maximum}.
    // this panel measures its text with fit(), then clamps the outer width to 40–100 pixels instead of growing.
    // limits follow box sizing. these panels use BorderBox, so their padding and borders are inside the limits.
    auto& bounded_fit = bounded_row.add<DemoPanel>("bounded-fit");
    bounded_fit.set_size({{fit(), px(40.0F), px(100.0F)}, grow()});
    bounded_fit.add<TextWidget>("fit");

    // along the row, grow reserves each child's minimum first, then divides remaining space by grow weight.
    // once bounded-grow reaches its percentage maximum, its unused share goes to this uncapped sibling.
    // pixel limits apply during intrinsic measurement too. percentage limits wait for parent allocation, so a fit
    // parent is not measured from a percentage of its own as-yet unknown size.
    auto& remainder = bounded_row.add<DemoPanel>("bounded-remainder");
    remainder.set_size({grow(), grow()});
    remainder.add<TextWidget>("remaining space");

    auto& profile = add<DemoPanel>("profile");
    profile.set_size({grow(), fit()});
    profile.set_spacing(8.0F);
    profile.add<TextWidget>("profile");
    auto& name_input = profile.add<DemoTextInput>(m_name, "name");
    name_input.set_label("name")
        .set_label_placement(LabelPlacement::Inline)
        .set_icon(ui.runtime().textures().find("demo-file-icon"));
    profile.add<DemoCheckbox>(m_enabled, "enabled").set_size({px(360.0F), px(32.0F)});

    // select the shape before attachment so the demo theme overrides its rounding last.
    auto radio = std::make_unique<DemoCheckbox>(m_radio_selected, "radio");
    radio->set_type(CheckboxType::Radio);
    profile.add(std::move(radio)).set_size({px(360.0F), px(32.0F)});

    profile.add<DemoColorPicker>(m_color, "color", "color-picker");

    m_test_images = &profile.add<Container>("demo-images", StackDirection::Horizontal);
    m_test_images->set_size({grow(), px(140.0F)});
    m_test_images->set_spacing(8.0F);
    m_test_images->set_scrollable(false, true);

    m_test_images->configure_all_styles([](Style& style) {
        style.box_shadow({
            .offset = {0.0F, 0.0F},
            .blur = 250.0F,
            .spread = 10.0F,
            .color = rgba(1.0F, 1.0F, 1.0F, 110.0F / 255.0F),
        });
    });

    add_test_image(ui.runtime().textures().find("demo-test-image"));
    add_test_image(ui.runtime().textures().find("demo-test-gif"));

    auto& add_image = profile.add<ButtonWidget>("add test image", LayoutSize{px(140.0F), px(36.0F)});
    add_image.on_click([this] { add_test_image(surface().runtime().textures().find("demo-test-image")); });

    auto& select_image = profile.add<FileDialogWidget>("add image from file", "select-image");
    select_image.on_event([this, &select_image](UiEvent& event) {
        if (event.type == EventType::Click) select_test_image(select_image);
    });

    auto& image_fit = profile.add<DemoDropdown>(
        m_image_fit, std::vector<DropdownOption>{{"fill", "fill"}, {"contain", "contain"}, {"cover", "cover"}}, "image-fit"
    );

    image_fit.set_label("image fit").set_size({px(280.0F), fit()});
    image_fit.on_change([this] { apply_image_fit(); });

    profile.add<DemoAnimatedText>("hover for cool animation");

    profile.add<TextWidget>("ellipsis: this text is longer than the available width")
        .set_overflow(TextOverflow::Ellipsis)
        .set_size({px(220.0F), px(20.0F)});
    profile.add<TextWidget>("clip: this text is longer than the available width").set_size({px(220.0F), px(20.0F)});

    auto& appearance = add<DemoPanel>("appearance");
    appearance.set_size({grow(), fit()});
    appearance.set_spacing(8.0F);
    appearance.add<TextWidget>("appearance");

    auto& theme = appearance.add<DemoDropdown>(
        m_theme, std::vector<DropdownOption>{{"default", "default"}, {"pastel", "pastel"}, {"material 3", "material"}}, "theme"
    );

    theme.set_label("theme").set_size({px(360.0F), fit()});
    theme.on_change([this] {
        // changing m_theme alone does not recolor existing widgets. set_theme applies the new defaults to the attached tree.
        // these demo panel subclasses also choose colors from s_demo_theme_variant, so set it before applying the theme.
        s_demo_theme_variant = demo_theme_variant(m_theme);
        surface().set_theme(make_demo_theme(m_theme));
    });

    auto& border_style = appearance.add<DemoDropdown>(
        m_border_style, std::vector<DropdownOption>{{"solid", "solid"}, {"dashed", "dashed"}, {"dotted", "dotted"}},
        "border-style"
    );

    border_style.set_label("border style").set_size({px(360.0F), fit()});
    border_style.on_change([this] {
        BorderStyle style = BorderStyle::Solid;
        if (m_border_style == "dashed") style = BorderStyle::Dashed;
        if (m_border_style == "dotted") style = BorderStyle::Dotted;

        apply_border_style(surface().root(), style);
    });

    auto& chromatic = add<DemoChromaticPanel>(ui.effects().register_effect<DemoChromaticCommand>(
        {render_demo_chromatic, initialize_demo_chromatic, nullptr, shutdown_demo_chromatic, &demo_chromatic_state}
    ));
    chromatic.add<TextWidget>("custom chromatic-aberration pass");

    auto& linear = add<DemoGradientPanel>(GradientType::Linear, "linear-gradient");
    linear.add<TextWidget>("linear gradient");

    auto& radial = add<DemoGradientPanel>(GradientType::Radial, "radial-gradient");
    radial.add<TextWidget>("radial gradient");

    auto& actions = add<DemoPanel>("actions");
    actions.set_size({grow(), fit()});
    actions.set_spacing(8.0F);
    actions.add<TextWidget>("actions");
    auto& status = actions.add<TextWidget>("no clicks yet");
    auto& button = actions.add<ButtonWidget>("click me", LayoutSize{px(140.0F), px(44.0F)});

    auto& list_section = add<DemoPanel>("list-section");
    list_section.set_size({grow(), fit()});
    list_section.set_spacing(8.0F);
    list_section.add<TextWidget>("dynamic stack layout");
    m_text_list = &list_section.add<DemoTextListWidget>(std::vector<std::string>{"first item", "second item", "third item"});

    auto& text_list_orientation = list_section.add<ButtonWidget>("list orientation: vertical", LayoutSize{px(240.0F), px(36.0F)});
    text_list_orientation.on_click([this, &text_list_orientation] {
        m_text_list_horizontal = !m_text_list_horizontal;
        m_text_list->set_direction(m_text_list_horizontal ? StackDirection::Horizontal : StackDirection::Vertical);
        text_list_orientation.set_text(m_text_list_horizontal ? "list orientation: horizontal" : "list orientation: vertical");
    });

    auto& virtual_section = add<DemoPanel>("virtual-section");
    virtual_section.set_size({grow(), fit()});
    virtual_section.set_spacing(8.0F);
    virtual_section.add<TextWidget>("virtual list (100000 items)");
    virtual_section.add<TextWidget>("click to expand or collapse it");

    auto& virtual_list = virtual_section.add<VirtualLayout>("demo-virtual-list", 32.0F);
    virtual_list.set_size({grow(), px(260.0F)});
    virtual_list.set_spacing(4.0F);
    virtual_list.set_overscan(5);

    // set_items supplies a row count and a callback that returns the node for an index. it does not create all rows upfront.
    // the callback is requested for visible rows and up to five rows on each side, as configured by set_overscan(5).
    // retain each created button in cache so revisiting an index returns the same node and preserves its state.
    virtual_list.set_items(
        100000, [&virtual_list, cache = std::unordered_map<size_t, ButtonWidget*>{}](size_t index) mutable -> Node& {
            const auto found = cache.find(index);

            if (found != cache.end()) {
                return *found->second;
            }

            auto& row = virtual_list.add<ButtonWidget>(std::format("item {} - expand", index + 1));
            row.set_id(std::format("virtual-row-{}", index));
            row.on_click([&virtual_list, &row, index] {
                const bool expanded = virtual_list.extra_offset(index) == 0.0F;

                // set_extra_offset adds to the row's normal extent. expanding adds 64 pixels and collapsing removes them.
                // the list shifts later rows and adjusts its scroll extent, so the extra text does not cover the next row.
                virtual_list.set_extra_offset(index, expanded ? 64.0F : 0.0F);
                row.set_text(
                    expanded ? std::format("item {} - collapse\nthis row reserves 64 extra pixels", index + 1)
                             : std::format("item {} - expand", index + 1)
                );
            });

            cache.emplace(index, &row);
            return row;
        }
    );

    button.on_click([this, &button, &status] {
        ++m_clicks;
        status.set_text(std::format("button clicks: {}", m_clicks));
        button.animate().by(StyleAnimationProperty::Rotation, 180.0F, {0.5F, easing::out_back});
    });
}

void DemoScreen::setup_dynamic_nodes(Node& parent) {
    auto& dynamic_section =
        parent.add<DemoPanel>("dynamic-section", DemoPanelTone::Base, ImVec2{14.0F, 14.0F}, BORDER_ALL, true, true);

    dynamic_section.set_direction(StackDirection::Horizontal);
    dynamic_section.set_spacing(8.0F);

    // anchor and origin both select TopRight. the panel's top-right sits 20 pixels left and 72 pixels below the overlay's.
    // in_flow=false affects this panel's relationship with its parent, not the layout of its own children.
    // it neither advances the parent's stack cursor nor enlarges a fit-sized parent. its children still stack horizontally.
    // fit/fit measures those children, so adding a wider child grows the panel leftward while its top-right stays anchored.
    dynamic_section.set_layout({
        .size = {fit(), fit()},
        .placement = {.anchor = Anchor::TopRight, .origin = Anchor::TopRight, .offset = {-20.0F, 72.0F}},
        .in_flow = false,
    });

    auto& node_controls = dynamic_section.add<Container>("dynamic-node-controls", StackDirection::Vertical);
    node_controls.set_size({fit(), fit()});
    node_controls.set_spacing(8.0F);

    auto& dynamic_list = dynamic_section.add<Container>("dynamic-list", StackDirection::Vertical);
    dynamic_list.set_size({px(300.0F), grow()});
    dynamic_list.set_spacing(8.0F);
    m_dynamic_status = &dynamic_list.add<TextWidget>("dynamic nodes: 0");
    dynamic_list.add<TextWidget>("click a list item to remove it");

    m_dynamic_nodes = &dynamic_list.add<DemoResizableNodes>("dynamic-nodes");
    m_dynamic_nodes->set_size({px(240.0F), grow()});
    m_dynamic_nodes->set_resize(ResizeAxes::Both).set_spacing(8.0F).set_scrollable(true);

    auto& add_node = node_controls.add<ButtonWidget>("add node", LayoutSize{px(120.0F), px(36.0F)});
    add_node.on_click([this] {
        ++m_dynamic_count;

        const int item_id = ++m_next_dynamic_id;
        auto& item = m_dynamic_nodes->add<ButtonWidget>(std::format("list item {}", item_id), LayoutSize{grow(), px(36.0F)});
        item.on_click([this, &item] {
            // remove marks the node for deletion and excludes it from input and layout. destruction is deferred until
            // a tree update, allowing this callback to finish without destroying its own button midway through dispatch.
            if (m_dynamic_nodes->remove(item)) {
                --m_dynamic_count;
                m_dynamic_status->set_text(std::format("dynamic nodes: {}", m_dynamic_count));
            }
        });

        m_dynamic_status->set_text(std::format("dynamic nodes: {}", m_dynamic_count));
    });

    auto& show_progress = node_controls.add<ButtonWidget>("show progress", LayoutSize{px(120.0F), px(36.0F)});
    show_progress.on_click([this] {
        ++m_dynamic_count;
        m_dynamic_nodes->add<DemoProgressWidget>();
        m_dynamic_status->set_text(std::format("dynamic nodes: {}", m_dynamic_count));
    });

    auto& remove_node = node_controls.add<ButtonWidget>("remove node", LayoutSize{px(120.0F), px(36.0F)});
    remove_node.on_click([this] {
        if (m_dynamic_nodes->children().empty()) {
            return;
        }

        m_dynamic_nodes->remove(*m_dynamic_nodes->children().back());

        --m_dynamic_count;
        m_dynamic_status->set_text(std::format("dynamic nodes: {}", m_dynamic_count));
    });

    auto& clear_nodes = node_controls.add<ButtonWidget>("clear nodes", LayoutSize{px(120.0F), px(36.0F)});
    clear_nodes.on_click([this] {
        m_dynamic_nodes->clear();
        m_dynamic_count = 0;
        m_dynamic_status->set_text("dynamic nodes: 0");
    });
}

ImageWidget& DemoScreen::add_test_image(Texture* texture) {
    auto& image = m_test_images->add<ImageWidget>(texture);
    image.set_size({grow(), grow()});
    image.set_fit(image_fit());

    // InputMode::None leaves an image out of pointer hit testing by default. Target lets it receive hover and click events.
    // Target does not exclude other overlapping branches. use Blocker when a layer must prevent input reaching content behind it.
    image.set_input_mode(InputMode::Target);

    ImageWidget* image_ptr = &image;
    image.on_event([this, image_ptr](UiEvent& event) {
        if (event.type != EventType::Click) {
            return;
        }

        image_ptr->set_enabled(false);

        // set_enabled(false) leaves the image drawn but stops it receiving another click while this sequence runs.
        // by adds to the property's starting value, while to chooses an absolute target. calls in a phase run together.
        // release_all animates properties back to their style values. end runs after that final phase completes.
        // the callback marks the image for deferred removal, so it is not destroyed while its animation callback is executing.
        image_ptr->animate()
            .by(StyleAnimationProperty::PaddingY, 12.0F, {0.30F, easing::in_out_sine})
            .to(StyleAnimationProperty::Scale, ImVec2{1.12F, 0.78F}, {0.30F, easing::in_out_sine})
            .then()
            .to(StyleAnimationProperty::PaddingY, 0.0F, {0.28F, easing::in_out_sine})
            .to(StyleAnimationProperty::Scale, ImVec2{0.94F, 1.08F}, {0.28F, easing::in_out_sine})
            .then()
            .to(StyleAnimationProperty::PaddingY, 3.0F, {0.22F, easing::in_out_sine})
            .to(StyleAnimationProperty::Scale, ImVec2{1.03F, 0.97F}, {0.22F, easing::in_out_sine})
            .then()
            .release_all({0.38F, easing::in_out_sine})
            .end([image_ptr] {
                if (Node* parent = image_ptr->parent(); parent != nullptr) {
                    parent->remove(*image_ptr);
                }
            });
    });

    return image;
}

void DemoScreen::select_test_image(FileDialogWidget& dialog) {
    static const std::array<FileDialogFilter, 1> image_filters{{{"image files", "png,jpg,jpeg,gif,svg"}}};

    const FileDialogResult result = dialog.show_dialog(FileDialogOperation::OpenFile, {.filters = image_filters});
    if (!result.accepted() || result.paths.empty()) {
        return;
    }

    Texture* texture = surface().runtime().textures().add(result.paths.front().filename().string(), result.paths.front());
    add_test_image(texture);
    dialog.set_value(result.paths.front().filename().string());
}

ImageFit DemoScreen::image_fit() const {
    if (m_image_fit == "contain") return ImageFit::Contain;
    if (m_image_fit == "cover") return ImageFit::Cover;

    return ImageFit::Fill;
}

void DemoScreen::apply_image_fit() {
    const ImageFit fit = image_fit();
    for (const auto& child : m_test_images->children()) {
        static_cast<ImageWidget*>(child.get())->set_fit(fit);
    }
}

int& DemoScreen::blur() {
    return m_blur;
}

void DemoScreen::on_update(float dt) {
    m_fps_update_elapsed += std::max(0.0F, dt);
    if (m_fps_update_elapsed >= 0.25F) {
        m_fps_update_elapsed = 0.0F;
        m_fps->set_text(std::format("fps: {:.1f}", ImGui::GetIO().Framerate));
    }
}

static void apply_border_style(Node& node, BorderStyle style) {
    if (auto* styled_node = dynamic_cast<StyledNode*>(&node)) {
        styled_node->configure_all_styles([style](Style& current_style) { current_style.border_style(style); });
    }

    for (const auto& child : node.children()) {
        apply_border_style(*child, style);
    }
}

void setup_demo(Surface& surface, std::string backend) {
    Runtime& runtime = surface.runtime();
#ifdef IMGUI_UI_ASSETS_DIR

    // register fonts before widgets request them. each font loads once per imgui context.
    const std::filesystem::path assets = std::filesystem::path{IMGUI_UI_ASSETS_DIR};
    runtime.fonts().add("Inter Regular", assets / "fonts/Inter.ttf");
    surface.set_primary_font(runtime.fonts().find("Inter Regular"));

    runtime.textures().add("demo-file-icon", assets / "icons/demo.svg");
    runtime.textures().add("demo-test-image", assets / "images/tiny.jpg");
    runtime.textures().add("demo-test-gif", assets / "images/t3.gif");
    runtime.textures().add("demo-inline-icon", DEMO_INLINE_ICON_SVG);
#endif
    Texture* inline_icon = runtime.textures().find("demo-inline-icon");

    auto& demo = surface.root().add<DemoScreen>();
    demo.setup(std::move(backend));

    surface.root().add<DemoFloatingWindow>();

    auto& overlay = demo.add<LayerContainer>("##demo-overlay");

    // LayerMode::Inline draws inside a child window over the parent's content box, rather than in a separate movable window.
    // layers default to in_flow=false. without an explicit size, this one fills the parent's content box and reserves no stack
    // space. the layer compensates for its parent's scroll position, keeping these controls in the demo viewport as the panels
    // scroll.
    demo.setup_dynamic_nodes(overlay);

    auto& context_status = demo.add<TextWidget>("context menu: no selection");
    auto& context_button = demo.add<ButtonWidget>("open context menu", LayoutSize{px(220.0F), px(40.0F)});
    ContextMenuItems context_items = {
        ContextMenuItem::action(
            "first action", [&context_status](auto&) { context_status.set_text("context menu: first action"); }
        ),
        ContextMenuItem::submenu(
            "more actions", {
                                ContextMenuItem::action("second action", [&context_status](auto&) {
                                    context_status.set_text("context menu: second action");
                                }),
                            }
        ),
    };

    auto& context_menu = surface.root().add<ContextMenuWidget>(std::move(context_items), inline_icon);
    context_menu.set_hover_close_delay(2.0f);

    context_button.on_click([&context_menu] { context_menu.open(); });

    // the full-size layer is the input barrier, while its child is only the visible centered panel.
    // InputMode::Blocker excludes unrelated branches behind the layer from pointer events, including native scrollbar
    // interaction. descendants remain eligible, so the panel's close button works. clicking outside the panel still hits the
    // backdrop instead of the demo underneath. hiding the layer removes that barrier and restores input to the demo.
    auto& input_blocker = surface.root().add<LayerContainer>("##input-blocker");
    input_blocker.set_visible(false);

    auto& blocker_panel = input_blocker.add<InputBlocker>();
    blocker_panel.add<TextWidget>("pointer input is blocked below this panel");

    auto& block_button = demo.add<ButtonWidget>("block pointer input", LayoutSize{px(220.0F), px(40.0F)});
    auto& unblock_button = blocker_panel.add<ButtonWidget>("disable pointer block", LayoutSize{px(284.0F), px(40.0F)});

    block_button.on_click([&input_blocker, &block_button] {
        input_blocker.set_visible(true);
        input_blocker.set_input_mode(InputMode::Blocker);
        block_button.set_text("pointer input blocked");
    });

    unblock_button.on_click([&input_blocker, &block_button] {
        input_blocker.set_input_mode(InputMode::None);
        input_blocker.set_visible(false);
        block_button.set_text("block pointer input");
    });

    // apply blur to the full-size backdrop rather than to the modal panel. it samples content drawn earlier behind the layer.
    // the panel is drawn as a descendant over that blurred background. Blocker makes the entire backdrop reject input to the
    // demo, including the transparent area outside the panel, so transparency does not make the modal pass input through.
    auto& modal_layer = surface.root().add<LayerContainer>("##modal-layer", LayerMode::Inline);
    modal_layer.set_visible(false);
    modal_layer.set_input_mode(InputMode::Blocker);
    modal_layer.configure_all_styles([](Style& style) { style.background_color(rgba(0.0F, 0.0F, 0.0F, 0.0F)).blur(5); });

    auto& modal = modal_layer.add<DemoPanel>("demo-modal", DemoPanelTone::Secondary, ImVec2{24.0F, 24.0F}, BORDER_ALL);
    modal.set_visible(false);

    // a fixed 480x220 panel is centered in the full-size backdrop by aligning Center with Center.
    // in_flow=false positions it independently of the backdrop's stack. it does not reserve a 220-pixel slot for later siblings.
    modal.set_layout({
        .size = {px(480.0F), px(220.0F)},
        .placement = {.anchor = Anchor::Center, .origin = Anchor::Center},
        .in_flow = false,
    });
    modal.set_spacing(10.0F);

    // close both the panel and backdrop on an outside left press or click, or escape. stop delivery to underlying targets.
    modal_layer.on_event([&modal_layer, &modal](UiEvent& event) {
        const bool clicked_outside = (event.type == EventType::Click || event.type == EventType::PointerDown) &&
                                     event.button == PointerButton::Left &&
                                     !modal.layout().visual_rect().contains(event.position);
        const bool pressed_escape =
            event.type == EventType::Cancel || (event.type == EventType::KeyDown && event.key == Key::Escape);
        if (!clicked_outside && !pressed_escape) {
            return;
        }

        modal.set_visible(false);
        modal_layer.set_visible(false);
        event.stop_propagation();
    });

    modal.add<TextWidget>("modal overlay");
    auto& blur = modal.add<NumberInputWidget>(demo.blur(), "modal-blur");
    blur.set_label("backdrop blur").set_range(0, 32);
    blur.on_change([&demo, &modal_layer] {
        modal_layer.configure_all_styles([&demo](Style& style) { style.blur(demo.blur()); });
    });

    auto& close_button = modal.add<ButtonWidget>("close modal", LayoutSize{px(180.0F), px(40.0F)});

    // set_anchor(anchor, origin) changes placement and sets in_flow=false, preserving size and the existing placement offset.
    // the one-argument overload uses the same anchor and origin. BottomCenter aligns the button's bottom-center with
    // the panel's content bottom-center, inside its padding. unlike a flowing footer, it reserves no space below the text.
    // adding enough flowing content can therefore overlap it. use an in-flow footer with a growing spacer to reserve that space.
    close_button.set_anchor(Anchor::BottomCenter, Anchor::BottomCenter);
    close_button.on_click([&modal_layer, &modal] {
        modal.set_visible(false);
        modal_layer.set_visible(false);
    });

    auto& modal_button = demo.add<ButtonWidget>("open modal", LayoutSize{px(220.0F), px(40.0F)});
    modal_button.on_click([&modal_layer, &modal, &surface] {
        if (modal.visible()) {
            return;
        }

        modal_layer.set_visible(true);
        modal.set_visible(true);

        // showing a layer does not give it keyboard focus. assign focus explicitly so escape reaches the backdrop's handler
        // immediately. once a descendant receives focus, its unhandled events can still bubble to that handler.
        surface.input_router().set_focus(&modal_layer);
    });
}
