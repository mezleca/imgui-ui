#include "demo.hpp"

#include <ui/backends/opengl/texture-loader.hpp>
#include <ui/layout/layer-container.hpp>
#include <ui/layout/resizable-container.hpp>
#include <ui/layout/virtual-layout.hpp>
#include <ui/resources/texture-registry.hpp>
#include <ui/style/style.hpp>
#include <ui/style/styled-node.hpp>
#include <ui/ui.hpp>
#include <ui/widgets/button.hpp>
#include <ui/widgets/checkbox.hpp>
#include <ui/widgets/color-picker.hpp>
#include <ui/widgets/context-menu.hpp>
#include <ui/widgets/dropdown.hpp>
#include <ui/widgets/image.hpp>
#include <ui/widgets/number-input.hpp>
#include <ui/widgets/text-input.hpp>
#include <ui/widgets/text.hpp>

#include <algorithm>
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

static Theme make_demo_theme(std::string_view variant) {
    Theme theme{};

    if (variant == "pastel") {
        theme.content_padding = 15.0F;
        theme.box_rounding = 6.0F;
        theme.controls.rounding = 10.0F;
        theme.checkbox_rounding = 6.0F;
        theme.controls.border_thickness = 1.0F;
        theme.controls.thumb_size = 14.0F;
        theme.accent_color = {226.0F / 255.0F, 180.0F / 255.0F, 189.0F / 255.0F, 1.0F};
        theme.accent_hover_color = {214.0F / 255.0F, 153.0F / 255.0F, 165.0F / 255.0F, 1.0F};
        theme.background_color = {255.0F / 255.0F, 245.0F / 255.0F, 245.0F / 255.0F, 1.0F};
        theme.background_secondary_color = {255.0F / 255.0F, 250.0F / 255.0F, 250.0F / 255.0F, 1.0F};
        theme.background_tertiary_color = {247.0F / 255.0F, 214.0F / 255.0F, 208.0F / 255.0F, 1.0F};
        theme.scrollbar_background_color = {226.0F / 255.0F, 180.0F / 255.0F, 189.0F / 255.0F, 0.55F};
        theme.header_background_color = {247.0F / 255.0F, 214.0F / 255.0F, 208.0F / 255.0F, 1.0F};
        theme.text_color = {74.0F / 255.0F, 74.0F / 255.0F, 74.0F / 255.0F, 1.0F};
        theme.text_secondary_color = {106.0F / 255.0F, 87.0F / 255.0F, 90.0F / 255.0F, 1.0F};
        theme.border_color = {226.0F / 255.0F, 180.0F / 255.0F, 189.0F / 255.0F, 1.0F};
        theme.header_border_color = {214.0F / 255.0F, 153.0F / 255.0F, 165.0F / 255.0F, 0.55F};
        theme.button_active_color = {226.0F / 255.0F, 180.0F / 255.0F, 189.0F / 255.0F, 0.35F};
        theme.controls.background_color = {255.0F / 255.0F, 245.0F / 255.0F, 245.0F / 255.0F, 1.0F};
        theme.controls.hover_color = {247.0F / 255.0F, 214.0F / 255.0F, 208.0F / 255.0F, 1.0F};
        theme.controls.active_color = {226.0F / 255.0F, 180.0F / 255.0F, 189.0F / 255.0F, 0.35F};
        theme.controls.border_color = {226.0F / 255.0F, 180.0F / 255.0F, 189.0F / 255.0F, 1.0F};
        theme.metrics.popup_rounding = 10.0F;
        theme.metrics.tab_rounding = 8.0F;
        theme.metrics.item_spacing = {12.0F, 10.0F};
        theme.widgets.dropdown_item_padding = {12.0F, 7.0F};
        theme.widgets.dropdown_arrow_size = {10.0F, 5.0F};
        theme.widgets.dropdown_popup_gap = 6.0F;
        theme.widgets.context_menu_width = 196.0F;
        theme.widgets.context_menu_item_height = 36.0F;
        theme.widgets.context_menu_padding = {8.0F, 8.0F};
        theme.widgets.context_menu_item_padding = {12.0F, 6.0F};
        theme.widgets.context_menu_gap = 8.0F;
        theme.widgets.context_menu_icon_size = 16.0F;
        theme.widgets.text_input_padding = {14.0F, 12.0F};
        theme.widgets.text_input_icon_size = {20.0F, 20.0F};
        theme.widgets.text_input_icon_spacing = 12.0F;
    } else if (variant == "material") {
        theme.content_padding = 18.0F;
        theme.box_rounding = 12.0F;
        theme.controls.rounding = 12.0F;
        theme.checkbox_rounding = 5.0F;
        theme.controls.border_thickness = 1.0F;
        theme.controls.thumb_size = 16.0F;
        theme.accent_color = {0.82F, 0.74F, 1.0F, 1.0F};
        theme.accent_hover_color = {0.91F, 0.87F, 0.97F, 1.0F};
        theme.background_color = {0.078F, 0.071F, 0.094F, 1.0F};
        theme.background_secondary_color = {0.129F, 0.122F, 0.149F, 1.0F};
        theme.background_tertiary_color = {0.059F, 0.051F, 0.075F, 1.0F};
        theme.scrollbar_background_color = {0.059F, 0.051F, 0.075F, 1.0F};
        theme.header_background_color = {0.169F, 0.161F, 0.188F, 1.0F};
        theme.text_color = {0.902F, 0.878F, 0.914F, 1.0F};
        theme.text_secondary_color = {0.792F, 0.769F, 0.816F, 1.0F};
        theme.border_color = {0.286F, 0.271F, 0.31F, 1.0F};
        theme.header_border_color = {0.475F, 0.455F, 0.494F, 0.35F};
        theme.button_active_color = {0.82F, 0.74F, 1.0F, 0.28F};
        theme.controls.background_color = {0.129F, 0.122F, 0.149F, 1.0F};
        theme.controls.hover_color = {0.212F, 0.196F, 0.239F, 1.0F};
        theme.controls.active_color = {0.82F, 0.74F, 1.0F, 0.24F};
        theme.controls.border_color = {0.475F, 0.455F, 0.494F, 1.0F};
        theme.metrics.window_rounding = 4.0F;
        theme.metrics.child_rounding = 4.0F;
        theme.metrics.popup_rounding = 12.0F;
        theme.metrics.tab_rounding = 8.0F;
        theme.metrics.frame_padding = {14.0F, 10.0F};
        theme.metrics.item_spacing = {12.0F, 12.0F};
        theme.metrics.item_inner_spacing = {8.0F, 8.0F};
        theme.widgets.dropdown_item_padding = {16.0F, 8.0F};
        theme.widgets.dropdown_arrow_size = {10.0F, 5.0F};
        theme.widgets.dropdown_popup_gap = 2.0F;
        theme.widgets.dropdown_transition_duration = 0.06F;
        theme.widgets.context_menu_width = 220.0F;
        theme.widgets.context_menu_item_height = 48.0F;
        theme.widgets.context_menu_padding = {8.0F, 8.0F};
        theme.widgets.context_menu_item_padding = {16.0F, 8.0F};
        theme.widgets.context_menu_gap = 4.0F;
        theme.widgets.context_menu_icon_size = 18.0F;
        theme.widgets.text_input_padding = {16.0F, 12.0F};
        theme.widgets.text_input_icon_size = {20.0F, 20.0F};
        theme.widgets.text_input_icon_spacing = 12.0F;
    }

    theme.controls.mark_color = theme.accent_color;
    return theme;
}

void configure_demo_runtime(RuntimeConfig& config) {
    config.theme = make_demo_theme("default");

#ifdef IMGUI_UI_ASSETS_DIR
    config.texture_loader = std::make_unique<OpenGLTextureLoader>();
#endif
}

enum class DemoPanelTone {
    Base,
    Secondary,
    Tertiary,
};

class DemoPanel : public StackContainer {
public:
    DemoPanel(
        std::string id, const Theme& theme, DemoPanelTone tone = DemoPanelTone::Base, ImVec2 padding = {14.0F, 14.0F},
        uint8_t border = BORDER_NONE, bool accent_border = false, bool shadow = false
    )
        : StackContainer(std::move(id)), m_tone(tone), m_padding(padding), m_border(border), m_accent_border(accent_border),
          m_shadow(shadow) {
        apply_theme_defaults(theme);
    }

protected:
    void apply_theme_defaults(const Theme& theme) override {
        const ImVec4& background = m_tone == DemoPanelTone::Secondary  ? theme.background_secondary_color
                                   : m_tone == DemoPanelTone::Tertiary ? theme.background_tertiary_color
                                                                       : theme.background_color;
        configure_all_styles([&](Style& style) {
            style.padding(m_padding)
                .background_color(background)
                .border(m_border)
                .border_color(m_accent_border ? theme.accent_color : theme.border_color)
                .border_radius(theme.box_rounding)
                .box_shadow(m_shadow ? BoxShadow{
                                           .offset = {0.0F, 8.0F},
                                           .blur = 18.0F,
                                           .spread = 2.0F,
                                           .color = ImColor{0.0F, 0.0F, 0.0F, 0.45F},
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

class DemoAccentButton final : public ButtonWidget {
public:
    DemoAccentButton(UI& ui, std::string text, LayoutSize size) : ButtonWidget(ui, std::move(text), size) {
        apply_theme_defaults(ui.theme());
    }

protected:
    void apply_theme_defaults(const Theme& theme) override {
        ButtonWidget::apply_theme_defaults(theme);

        configure_all_styles([&theme](Style& style) {
            style.background_color(theme.accent_color).border_color(theme.accent_hover_color);
        });

        configure_style(StyleType::HOVER, [&theme](Style& style) {
            style.background_color(theme.accent_hover_color).border_color(theme.accent_hover_color);
        });

        configure_style(StyleType::ACTIVE, [&theme](Style& style) {
            style.background_color(theme.accent_color).border_color(theme.accent_color);
        });
    }
};

class DemoAnimatedText final : public TextWidget {
public:
    DemoAnimatedText(UI& ui, std::string text) : TextWidget(std::move(text)), m_ui(ui) {
        set_input_mode(InputMode::Target);
        apply_theme_defaults(ui.theme());
    }

protected:
    void apply_theme_defaults(const Theme& theme) override {
        TextWidget::apply_theme_defaults(theme);
        configure_all_styles([](Style& style) { style.padding({}).background_color({}); });
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
            animate()
                .padding_y(10.0F, {0.2F, easing::out_quad})
                .then(0.1F)
                .padding_x(20.0F, {0.24F, easing::out_cubic})
                .background_color(m_ui.theme().accent_color, {0.24F, easing::out_cubic})
                .then(0.1F)
                .rotation(180.0F, {0.25F, easing::out_cubic});
        } else {
            animate().release_all({0.15F, easing::linear});
        }
    }

    UI& m_ui;
    bool m_hovered = false;
};

static void apply_border_style(Node& node, BorderStyle style);

class DemoProgressWidget final : public DrawListWidget {
public:
    explicit DemoProgressWidget(UI& surface) : DrawListWidget("demo-progress") {
        set_size({grow(), px(68.0F)});
        set_font(surface.get_primary_font(14));
        apply_theme_defaults(surface.theme());
    }

protected:
    void apply_theme_defaults(const Theme& theme) override {
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
    void paint_draw_list(ImDrawList& draw_list, Rect rect, const ComputedStyle& style) override {
        const Rect box =
            Rect::from_position_size({rect.min.x + 6.0F, rect.min.y + 6.0F}, {std::max(0.0F, rect.size().x - 12.0F), 32.0F});
        const float fill_right = std::min(box.max.x, box.min.x + m_fill_width);

        draw_list.AddRectFilled(box.min, box.max, style.background_color().get_col(), style.border_radius());
        if (fill_right > box.min.x) {
            draw_list.AddRectFilled(
                box.min, {fill_right, box.max.y}, ImGui::GetColorU32(m_fill_color.Value), style.border_radius()
            );
        }
        draw_list.AddRect(
            box.min, box.max, style.border_color().get_col(), style.border_radius(), ImDrawFlags_RoundCornersAll,
            style.border_thickness()
        );

        if (!m_completed) {
            return;
        }

        ImColor message_color = style.color().value;
        message_color.Value.w *= m_message_opacity;
        const ImVec2 message_size = ImGui::CalcTextSize("finished");
        draw_list.AddText(
            {box.min.x + (box.size().x - message_size.x) * 0.5F, box.min.y + 8.0F + m_message_offset},
            ImGui::GetColorU32(message_color.Value), "finished"
        );
    }

    ImColor m_fill_color;
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
    DemoResizableNodes(std::string id, const Theme& theme) : ResizableContainer(std::move(id)) {
        apply_theme_defaults(theme);
    }

protected:
    void apply_theme_defaults(const Theme& theme) override {
        configure_all_styles([&theme](Style& style) {
            style.background_color(theme.background_tertiary_color)
                .border(BORDER_NONE)
                .border_radius(theme.box_rounding)
                .padding({20.0F, 20.0F})
                .cursor(ImGuiMouseCursor_ResizeNWSE);
        });
    }
};

class DemoTextListWidget final : public StackContainer {
public:
    DemoTextListWidget(const Theme& theme, std::vector<std::string> items);
    DemoTextListWidget& set_items(std::vector<std::string> items);

protected:
    void apply_theme_defaults(const Theme& theme) override {
        configure_all_styles([&theme](Style& style) {
            style.padding({8.0F, 8.0F})
                .background_color(theme.background_tertiary_color)
                .border(BORDER_NONE)
                .border_radius(theme.box_rounding);
        });
    }
};

class DemoScreen final : public StackContainer {
public:
    DemoScreen(UI& surface, std::string backend);
    void setup_dynamic_nodes(Node& parent);
    int& blur();

private:
    ImageWidget& add_test_image(Texture* texture);
    ImageFit image_fit() const;
    void apply_image_fit();
    void on_update(float dt) override;

protected:
    void apply_theme_defaults(const Theme& theme) override {
        configure_all_styles([&theme](Style& style) {
            style.padding({theme.content_padding, theme.content_padding}).background_color(theme.background_secondary_color);
        });
    }

private:
    UI& m_surface;
    ResizableContainer* m_dynamic_nodes = nullptr;
    TextWidget* m_dynamic_status = nullptr;
    TextWidget* m_fps = nullptr;
    StackContainer* m_test_images = nullptr;
    std::vector<Node*> m_pending_image_removals;
    Node* m_pending_remove = nullptr;
    bool m_enabled = true;
    ImColor m_color = {0.26F, 0.59F, 0.98F, 1.0F};
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

DemoTextListWidget::DemoTextListWidget(const Theme& theme, std::vector<std::string> items) : StackContainer("demo-text-list") {
    set_spacing(8.0F);
    set_size({fit(), fit()});
    apply_theme_defaults(theme);
    set_items(std::move(items));
}

DemoTextListWidget& DemoTextListWidget::set_items(std::vector<std::string> items) {
    clear();
    for (std::string& item : items) {
        add<TextWidget>(std::move(item));
    }
    return *this;
}

DemoScreen::DemoScreen(UI& surface, std::string backend) : StackContainer("demo", StackDirection::Vertical), m_surface(surface) {
    set_size({grow(), grow()});
    set_scrollable(true);
    set_spacing(16.0F);
    apply_theme_defaults(surface.theme());

    auto& overview = add<DemoPanel>("overview", surface.theme());
    overview.set_size({grow(), fit()});
    overview.set_spacing(4.0F);
    overview.add<TextWidget>("imgui-ui example");

    overview.add<TextWidget>(std::format("backend: {}", backend));
    m_fps = &overview.add<TextWidget>("fps: 0.0");
    overview.add<TextWidget>("debugger: shift + d");
    auto& random_slider = overview.add<NumberInputWidget>(m_surface, m_random_value);
    random_slider.set_label("random value");
    random_slider.set_maximum(10);

    auto& profile = add<DemoPanel>("profile", surface.theme());
    profile.set_size({grow(), fit()});
    profile.set_spacing(8.0F);
    profile.add<TextWidget>("profile");
    auto& name_input = profile.add<TextInputWidget>(surface, m_name, "name");
    name_input.set_size({px(360.0F), px(42.0F)});
    name_input.set_icon(m_surface.runtime().textures().find("demo-file-icon"));
    profile.add<CheckboxWidget>(surface, m_enabled, "enabled").set_size({px(360.0F), px(32.0F)});
    profile.add<ColorPickerWidget>(surface, m_color, "color", "color-picker");

    m_test_images = &profile.add<StackContainer>("demo-images", StackDirection::Horizontal);
    m_test_images->set_size({grow(), px(140.0F)});
    m_test_images->set_spacing(8.0F);

    m_test_images->configure_all_styles([](Style& style) {
        style.box_shadow({
            .offset = {0.0F, 0.0F},
            .blur = 250.0F,
            .spread = 10.0F,
            .color = ImColor(255, 255, 255, 110),
        });
    });

    add_test_image(m_surface.runtime().textures().find("demo-test-image"));
    add_test_image(m_surface.runtime().textures().find("demo-test-gif"));

    auto& add_image = profile.add<ButtonWidget>(surface, "add image", LayoutSize{px(140.0F), px(36.0F)});
    add_image.set_on_click([this] { add_test_image(m_surface.runtime().textures().find("demo-test-image")); });

    auto& image_fit = profile.add<DropdownWidget>(
        surface, m_image_fit, std::vector<DropdownOption>{{"fill", "fill"}, {"contain", "contain"}, {"cover", "cover"}},
        "image-fit"
    );

    image_fit.set_label("image fit").set_size({px(280.0F), px(68.0F)});
    image_fit.set_on_change([this] { apply_image_fit(); });

    profile.add<DemoAnimatedText>(surface, "hover for cool animation");

    profile.add<TextWidget>("ellipsis: this text is longer than the available width")
        .set_size({px(220.0F), px(20.0F)})
        .set_overflow(TextOverflow::Ellipsis);
    profile.add<TextWidget>("clip: this text is longer than the available width").set_size({px(220.0F), px(20.0F)});

    auto& appearance = add<DemoPanel>("appearance", surface.theme());
    appearance.set_size({grow(), fit()});
    appearance.set_spacing(8.0F);
    appearance.add<TextWidget>("appearance");

    auto& theme = appearance.add<DropdownWidget>(
        surface, m_theme, std::vector<DropdownOption>{{"default", "default"}, {"pastel", "pastel"}, {"material 3", "material"}},
        "theme"
    );

    theme.set_label("theme").set_size({px(360.0F), px(68.0F)});
    theme.set_on_change([this] { m_surface.set_theme(make_demo_theme(m_theme)); });

    auto& border_style = appearance.add<DropdownWidget>(
        surface, m_border_style, std::vector<DropdownOption>{{"solid", "solid"}, {"dashed", "dashed"}, {"dotted", "dotted"}},
        "border-style"
    );

    border_style.set_label("border style").set_size({px(360.0F), px(68.0F)});
    border_style.set_on_change([this] {
        const BorderStyle style = m_border_style == "dashed"   ? BorderStyle::Dashed
                                  : m_border_style == "dotted" ? BorderStyle::Dotted
                                                               : BorderStyle::Solid;
        apply_border_style(m_surface.root(), style);
    });

    auto& actions = add<DemoPanel>("actions", surface.theme());
    actions.set_size({grow(), fit()});
    actions.set_spacing(8.0F);
    actions.add<TextWidget>("actions");
    auto& status = actions.add<TextWidget>("no clicks yet");
    auto& button = actions.add<ButtonWidget>(surface, "click me", LayoutSize{px(140.0F), px(44.0F)});

    auto& list_section = add<DemoPanel>("list-section", surface.theme());
    list_section.set_size({grow(), fit()});
    list_section.set_spacing(8.0F);
    list_section.add<TextWidget>("dynamic stack layout");
    m_text_list = &list_section.add<DemoTextListWidget>(
        surface.theme(), std::vector<std::string>{"first item", "second item", "third item"}
    );

    auto& text_list_orientation =
        list_section.add<ButtonWidget>(surface, "list orientation: vertical", LayoutSize{px(240.0F), px(36.0F)});
    text_list_orientation.set_on_click([this, &text_list_orientation] {
        m_text_list_horizontal = !m_text_list_horizontal;
        m_text_list->set_direction(m_text_list_horizontal ? StackDirection::Horizontal : StackDirection::Vertical);
        text_list_orientation.set_text(m_text_list_horizontal ? "list orientation: horizontal" : "list orientation: vertical");
    });

    auto& virtual_section = add<DemoPanel>("virtual-section", surface.theme());
    virtual_section.set_size({grow(), fit()});
    virtual_section.set_spacing(8.0F);
    virtual_section.add<TextWidget>("virtual list (100000 items)");
    virtual_section.add<TextWidget>("click to expand or collapse it");

    auto& virtual_list = virtual_section.add<VirtualLayout>("demo-virtual-list", 32.0F);
    virtual_list.set_size({grow(), px(260.0F)});
    virtual_list.set_spacing(4.0F);
    virtual_list.set_overscan(5);
    virtual_list.set_items(
        100000, [&surface, &virtual_list, cache = std::unordered_map<size_t, ButtonWidget*>{}](size_t index) mutable -> Node& {
            const auto found = cache.find(index);

            if (found != cache.end()) {
                return *found->second;
            }

            auto& row = virtual_list.add<ButtonWidget>(surface, std::format("item {} - expand", index + 1));
            row.set_id(std::format("virtual-row-{}", index));
            row.set_on_click([&virtual_list, &row, index] {
                const bool expanded = virtual_list.extra_offset(index) == 0.0F;
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

    button.set_on_click([this, &button, &status] {
        ++m_clicks;
        status.set_text(std::format("button clicks: {}", m_clicks));
        button.animate().rotation_by(180.0F, {0.5F, easing::out_back});
    });
}

void DemoScreen::setup_dynamic_nodes(Node& parent) {
    auto& dynamic_section = parent.add<DemoPanel>(
        "dynamic-section", m_surface.theme(), DemoPanelTone::Base, ImVec2{14.0F, 14.0F}, BORDER_ALL, true, true
    );

    dynamic_section.set_direction(StackDirection::Horizontal);
    dynamic_section.set_spacing(8.0F);
    dynamic_section.set_layout({
        .size = {px(460.0F), px(220.0F)},
        .placement = {.anchor = Anchor::TopRight, .origin = Anchor::TopRight, .offset = {-20.0F, 72.0F}},
        .in_flow = false,
    });

    auto& node_controls = dynamic_section.add<StackContainer>("dynamic-node-controls", StackDirection::Vertical);
    node_controls.set_size({px(120.0F), grow()});
    node_controls.set_spacing(8.0F);

    auto& dynamic_list = dynamic_section.add<StackContainer>("dynamic-list", StackDirection::Vertical);
    dynamic_list.set_size({px(300.0F), grow()});
    dynamic_list.set_spacing(8.0F);
    m_dynamic_status = &dynamic_list.add<TextWidget>("dynamic nodes: 0");
    dynamic_list.add<TextWidget>("click a list item to remove it");

    // keep scrolling and resizing on the container so row nodes only describe content.
    m_dynamic_nodes = &dynamic_list.add<DemoResizableNodes>("dynamic-nodes", m_surface.theme());
    m_dynamic_nodes->set_size({px(240.0F), grow()});
    m_dynamic_nodes->set_resize(ResizeAxes::Both).set_spacing(8.0F).set_scrollable(true);

    auto& add_node = node_controls.add<ButtonWidget>(m_surface, "add node", LayoutSize{px(120.0F), px(36.0F)});
    add_node.set_on_click([this] {
        ++m_dynamic_count;

        const int item_id = ++m_next_dynamic_id;
        auto& item =
            m_dynamic_nodes->add<ButtonWidget>(m_surface, std::format("list item {}", item_id), LayoutSize{grow(), px(36.0F)});
        ButtonWidget* item_ptr = &item;
        item.set_on_click([this, item_ptr] { m_pending_remove = item_ptr; });

        m_dynamic_status->set_text(std::format("dynamic nodes: {}", m_dynamic_count));
    });

    auto& show_progress = node_controls.add<ButtonWidget>(m_surface, "show progress", LayoutSize{px(120.0F), px(36.0F)});
    show_progress.set_on_click([this] {
        ++m_dynamic_count;
        m_dynamic_nodes->add<DemoProgressWidget>(m_surface);
        m_dynamic_status->set_text(std::format("dynamic nodes: {}", m_dynamic_count));
    });

    auto& remove_node = node_controls.add<ButtonWidget>(m_surface, "remove node", LayoutSize{px(120.0F), px(36.0F)});
    remove_node.set_on_click([this] {
        if (m_dynamic_nodes->children().empty()) {
            return;
        }

        m_pending_remove = nullptr;
        m_dynamic_nodes->remove(*m_dynamic_nodes->children().back());

        --m_dynamic_count;
        m_dynamic_status->set_text(std::format("dynamic nodes: {}", m_dynamic_count));
    });

    auto& clear_nodes = node_controls.add<ButtonWidget>(m_surface, "clear nodes", LayoutSize{px(120.0F), px(36.0F)});
    clear_nodes.set_on_click([this] {
        m_pending_remove = nullptr;
        m_dynamic_nodes->clear();
        m_dynamic_count = 0;
        m_dynamic_status->set_text("dynamic nodes: 0");
    });
}

ImageWidget& DemoScreen::add_test_image(Texture* texture) {
    auto& image = m_test_images->add<ImageWidget>(texture);
    image.set_size({px(280.0F), px(140.0F)});
    image.set_fit(image_fit());
    image.set_input_mode(InputMode::Target);

    ImageWidget* image_ptr = &image;
    image.set_on_event([this, image_ptr](UiEvent& event) {
        if (event.type != EventType::Click) {
            return;
        }

        image_ptr->set_enabled(false);
        image_ptr->animate()
            .padding_y_by(12.0F, {0.30F, easing::in_out_sine})
            .scale({1.12F, 0.78F}, {0.30F, easing::in_out_sine})
            .then()
            .padding_y(0.0F, {0.28F, easing::in_out_sine})
            .scale({0.94F, 1.08F}, {0.28F, easing::in_out_sine})
            .then()
            .padding_y(3.0F, {0.22F, easing::in_out_sine})
            .scale({1.03F, 0.97F}, {0.22F, easing::in_out_sine})
            .then()
            .release_all({0.38F, easing::in_out_sine})
            .end([this, image_ptr] { m_pending_image_removals.push_back(image_ptr); });
    });

    return image;
}

ImageFit DemoScreen::image_fit() const {
    return m_image_fit == "contain" ? ImageFit::Contain : m_image_fit == "cover" ? ImageFit::Cover : ImageFit::Fill;
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

void DemoScreen::on_update(float) {
    m_fps->set_text(std::format("fps: {:.1f}", ImGui::GetIO().Framerate));

    for (Node* image : m_pending_image_removals) {
        if (image->parent() != nullptr) {
            image->parent()->remove(*image);
        }
    }
    m_pending_image_removals.clear();

    // defer destruction until dispatch finishes because the click callback still references the item.
    if (m_pending_remove != nullptr) {
        Node* pending_remove = m_pending_remove;
        m_pending_remove = nullptr;
        if (m_dynamic_nodes->contains(pending_remove)) {
            m_dynamic_nodes->remove(*pending_remove);
            --m_dynamic_count;
            m_dynamic_status->set_text(std::format("dynamic nodes: {}", m_dynamic_count));
        }
    }
}

static void apply_border_style(Node& node, BorderStyle style) {
    // walk descendants because each node stores its own style slots.
    if (auto* styled_node = dynamic_cast<StyledNode*>(&node)) {
        styled_node->configure_all_styles([style](Style& current_style) { current_style.border_style(style); });
    }

    for (const auto& child : node.children()) {
        apply_border_style(*child, style);
    }
}

void setup_demo(UI& surface, std::string backend) {
    Runtime& runtime = surface.runtime();
#ifdef IMGUI_UI_ASSETS_DIR
    // register paths before widgets request fonts. sizes load lazily per imgui context.
    const std::filesystem::path assets = std::filesystem::path{IMGUI_UI_ASSETS_DIR};
    runtime.fonts().add("Inter Regular", assets / "fonts/Inter.ttf");
    runtime.fonts().add("Inter SemiBold", assets / "fonts/Inter.ttf");
    runtime.fonts().add("Inter Bold", assets / "fonts/Inter.ttf");
    surface.set_primary_font(runtime.fonts().find("Inter Regular"));
    surface.set_secondary_font(runtime.fonts().find("Inter SemiBold"));

    runtime.textures().add("demo-file-icon", assets / "icons/demo.svg");
    runtime.textures().add("demo-test-image", assets / "images/tiny.jpg");
    runtime.textures().add("demo-test-gif", assets / "images/t3.gif");
    runtime.textures().add("demo-inline-icon", DEMO_INLINE_ICON_SVG);
#endif
    Texture* inline_icon = runtime.textures().find("demo-inline-icon");

    auto& demo = surface.root().add<DemoScreen>(surface, std::move(backend));

    auto& overlay = demo.add<LayerContainer>("##demo-overlay");
    auto& panel = overlay.add<DemoPanel>("overlay-panel", surface.theme());

    panel.set_layout({
        .size = {fit(), fit()},
        .placement = {.anchor = Anchor::TopRight, .origin = Anchor::TopRight, .offset = {-(460.0F + 12.0F + 20.0F), 72.0F}},
        .in_flow = false,
    });
    panel.add<TextWidget>("this panel is on the overlay layer");
    panel.set_visible(false);

    demo.setup_dynamic_nodes(overlay);

    auto& overlay_button = overlay.add<DemoAccentButton>(surface, "show overlay", LayoutSize{px(160.0F), px(40.0F)});

    overlay_button.set_layout({
        .size = {px(160.0F), px(40.0F)},
        .placement = {.anchor = Anchor::TopRight, .origin = Anchor::TopRight, .offset = {-20.0F, 20.0F}},
        .in_flow = false,
    });
    overlay_button.set_on_click([&overlay_button, &panel] {
        panel.set_visible(!panel.visible());
        overlay_button.set_text(panel.visible() ? "hide overlay" : "show overlay");
    });

    auto& context_status = demo.add<TextWidget>("context menu: no selection");
    auto& context_button = demo.add<ButtonWidget>(surface, "open context menu", LayoutSize{px(220.0F), px(40.0F)});
    ContextMenuItems context_items = {
        ContextMenuItem::action(
            "first action", [&context_status](auto&) { context_status.set_text("context menu: first action"); }
        ),
        ContextMenuItem::submenu("more actions", {ContextMenuItem::action("second action", [&context_status](auto&) {
                                     context_status.set_text("context menu: second action");
                                 })}),
    };

    auto& context_menu = surface.root().add<ContextMenuWidget>(surface, std::move(context_items), inline_icon);
    context_menu.set_hover_close_delay(2.0f);

    context_button.set_on_click([&context_menu] { context_menu.open(); });

    // block outside the panel so background controls cannot receive its input.
    auto& input_blocker = surface.root().add<LayerContainer>("##input-blocker");
    input_blocker.set_visible(false);

    auto& blocker_panel = input_blocker.add<DemoPanel>("input-blocker-panel", surface.theme());

    blocker_panel.set_layout({
        .size = {px(320.0F), px(150.0F)},
        .placement = {.anchor = Anchor::Center, .origin = Anchor::Center},
        .in_flow = false,
    });
    blocker_panel.set_spacing(10.0F);
    blocker_panel.add<TextWidget>("pointer input is blocked below this panel");

    auto& block_button = demo.add<ButtonWidget>(surface, "block pointer input", LayoutSize{px(220.0F), px(40.0F)});
    auto& unblock_button = blocker_panel.add<ButtonWidget>(surface, "disable pointer block", LayoutSize{px(284.0F), px(40.0F)});

    LayerContainer* blocker_ptr = &input_blocker;
    ButtonWidget* block_button_ptr = &block_button;
    block_button.set_on_click([blocker_ptr, block_button_ptr] {
        blocker_ptr->set_visible(true);
        // scope the blocker to the layer so its panel still receives clicks.
        blocker_ptr->set_input_mode(InputMode::Blocker);
        block_button_ptr->set_text("pointer input blocked");
    });

    unblock_button.set_on_click([blocker_ptr, block_button_ptr] {
        blocker_ptr->set_input_mode(InputMode::None);
        blocker_ptr->set_visible(false);
        block_button_ptr->set_text("block pointer input");
    });

    // sample the app behind this layer while routing modal input to the panel.
    auto& modal_layer = surface.root().add<LayerContainer>("##modal-layer", LayerMode::Inline);
    modal_layer.set_visible(false);
    modal_layer.set_input_mode(InputMode::Blocker);
    modal_layer.configure_all_styles([](Style& style) { style.background_color(ImColor{0.0F, 0.0F, 0.0F, 0.0F}).blur(5); });

    auto& modal =
        modal_layer.add<DemoPanel>("demo-modal", surface.theme(), DemoPanelTone::Secondary, ImVec2{24.0F, 24.0F}, BORDER_ALL);
    modal.set_visible(false);
    modal.set_layout({
        .size = {px(480.0F), px(220.0F)},
        .placement = {.anchor = Anchor::Center, .origin = Anchor::Center},
        .in_flow = false,
    });
    modal.set_spacing(10.0F);
    // keep backdrop dismissal on the layer. the panel handles its own controls.
    modal_layer.set_on_event([&modal_layer, &modal](UiEvent& event) {
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
    auto& blur = modal.add<NumberInputWidget>(surface, demo.blur(), "modal-blur");
    blur.set_label("backdrop blur").set_range(0, 32).set_size({px(180.0F), px(48.0F)});
    blur.set_on_change([&demo, &modal_layer] {
        modal_layer.configure_all_styles([&demo](Style& style) { style.blur(demo.blur()); });
    });

    auto& close_button = modal.add<ButtonWidget>(surface, "close modal", LayoutSize{px(180.0F), px(40.0F)});
    close_button.set_on_click([&modal_layer, &modal] {
        modal.set_visible(false);
        modal_layer.set_visible(false);
    });

    auto& modal_button = demo.add<ButtonWidget>(surface, "open modal", LayoutSize{px(220.0F), px(40.0F)});
    modal_button.set_on_click([&modal_layer, &modal, &surface] {
        if (modal.visible()) {
            return;
        }

        modal_layer.set_visible(true);
        modal.set_visible(true);
        surface.input_router().set_focus(modal_layer);
    });

    // refresh defaults after attaching text nodes whose constructors do not receive runtime.
    surface.set_theme(surface.theme());
}
