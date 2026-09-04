#include "demo.hpp"

#include <ui/backends/opengl/texture-loader.hpp>
#include <ui/layout/layer-container.hpp>
#include <ui/layout/resizable-container.hpp>
#include <ui/resources/texture-registry.hpp>
#include <ui/style/style.hpp>
#include <ui/style/styled-node.hpp>
#include <ui/ui.hpp>
#include <ui/widgets/button.hpp>
#include <ui/widgets/checkbox.hpp>
#include <ui/widgets/context-menu.hpp>
#include <ui/widgets/dropdown.hpp>
#include <ui/widgets/image.hpp>
#include <ui/widgets/number-input.hpp>
#include <ui/widgets/text-input.hpp>
#include <ui/widgets/text.hpp>

#include <filesystem>
#include <format>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

static constexpr std::string_view DEMO_INLINE_ICON_SVG = R"(
    <svg width="24" height="24" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
        <circle cx="12" cy="12" r="8" stroke="white" stroke-width="2"/>
        <path d="M12 8V12L15 14" stroke="white" stroke-width="2" stroke-linecap="round"/>
    </svg>)";

static ui::Theme make_demo_theme(bool dark) {
    ui::Theme theme = ui::Theme::defaults();
    theme.content_padding = 24.0F;
    theme.box_rounding = 8.0F;
    theme.control_rounding = 6.0F;

    if (!dark) {
        theme.accent_color = {0.35F, 0.65F, 1.0F, 1.0F};
        theme.accent_hover_color = {0.55F, 0.78F, 1.0F, 1.0F};
        theme.background_color = {0.08F, 0.09F, 0.12F, 1.0F};
        theme.background_secondary_color = {0.12F, 0.14F, 0.19F, 1.0F};
        theme.background_tertiary_color = {0.06F, 0.07F, 0.10F, 1.0F};
        theme.control_background_color = {0.10F, 0.12F, 0.17F, 1.0F};
        theme.control_hover_color = {0.16F, 0.20F, 0.28F, 1.0F};
        theme.control_active_color = {0.35F, 0.65F, 1.0F, 0.22F};
        theme.control_border_color = {0.26F, 0.32F, 0.42F, 1.0F};
        theme.border_color = {0.20F, 0.24F, 0.32F, 1.0F};
        theme.control_mark_color = theme.accent_color;
        return theme;
    }

    theme.accent_color = {1.0F, 0.62F, 0.22F, 1.0F};
    theme.accent_hover_color = {1.0F, 0.80F, 0.42F, 1.0F};
    theme.background_color = {0.025F, 0.028F, 0.038F, 1.0F};
    theme.background_secondary_color = {0.055F, 0.062F, 0.080F, 1.0F};
    theme.background_tertiary_color = {0.012F, 0.016F, 0.024F, 1.0F};
    theme.scrollbar_background_color = {0.008F, 0.010F, 0.016F, 1.0F};
    theme.header_background_color = {0.075F, 0.085F, 0.11F, 1.0F};
    theme.text_color = {0.96F, 0.97F, 0.99F, 1.0F};
    theme.text_secondary_color = {0.62F, 0.67F, 0.76F, 1.0F};
    theme.border_color = {0.13F, 0.16F, 0.21F, 1.0F};
    theme.header_border_color = {0.34F, 0.38F, 0.48F, 0.35F};
    theme.button_active_color = {1.0F, 0.62F, 0.22F, 0.30F};
    theme.control_background_color = {0.040F, 0.048F, 0.064F, 1.0F};
    theme.control_hover_color = {0.105F, 0.12F, 0.16F, 1.0F};
    theme.control_active_color = {1.0F, 0.62F, 0.22F, 0.25F};
    theme.control_border_color = {0.22F, 0.26F, 0.34F, 1.0F};
    theme.control_mark_color = theme.accent_color;
    return theme;
}

void configure_demo_runtime(ui::RuntimeConfig& config) {
    config.theme = make_demo_theme(false);

#ifdef IMGUI_UI_ASSETS_DIR
    config.texture_loader = std::make_unique<ui::OpenGLTextureLoader>();
#endif
}

enum class DemoPanelTone {
    Base,
    Secondary,
    Tertiary,
};

// gives demo-only panels one theme-aware style instead of styling them by id.
class DemoPanel : public ui::StackContainer {
public:
    DemoPanel(
        std::string id, const ui::Theme& theme, DemoPanelTone tone = DemoPanelTone::Base, ImVec2 padding = {14.0F, 14.0F},
        uint8_t border = ui::BORDER_NONE, bool accent_border = false, bool shadow = false
    )
        : ui::StackContainer(std::move(id)), m_tone(tone), m_padding(padding), m_border(border), m_accent_border(accent_border),
          m_shadow(shadow) {
        apply_theme_defaults(theme);
    }

protected:
    void apply_theme_defaults(const ui::Theme& theme) override {
        const ImVec4& background = m_tone == DemoPanelTone::Secondary  ? theme.background_secondary_color
                                   : m_tone == DemoPanelTone::Tertiary ? theme.background_tertiary_color
                                                                       : theme.background_color;
        configure_all_styles([&](ui::Style& style) {
            style.padding(m_padding)
                .background_color(background)
                .border(m_border)
                .border_color(m_accent_border ? theme.accent_color : theme.border_color)
                .border_radius(6.0F)
                .box_shadow(m_shadow ? ui::BoxShadow{
                                           .offset = {0.0F, 8.0F},
                                           .blur = 18.0F,
                                           .spread = 2.0F,
                                           .color = ImColor{0.0F, 0.0F, 0.0F, 0.45F},
                                       }
                                 : ui::BoxShadow{});
        });
    }

private:
    DemoPanelTone m_tone;
    ImVec2 m_padding;
    uint8_t m_border;
    bool m_accent_border;
    bool m_shadow;
};

// keeps the overlay action visually distinct while inheriting button behavior and sizing.
class DemoAccentButton final : public ui::ButtonWidget {
public:
    DemoAccentButton(UI& ui, std::string text, ui::LayoutSize size) : ui::ButtonWidget(ui, std::move(text), size) {
        apply_theme_defaults(ui.theme());
    }

protected:
    void apply_theme_defaults(const ui::Theme& theme) override {
        ui::ButtonWidget::apply_theme_defaults(theme);
        configure_all_styles([&theme](ui::Style& style) {
            style.background_color(theme.accent_color).border_color(theme.accent_hover_color);
        });
        configure_style(ui::StyleType::HOVER, [&theme](ui::Style& style) {
            style.background_color(theme.accent_hover_color).border_color(theme.accent_hover_color);
        });
        configure_style(ui::StyleType::ACTIVE, [&theme](ui::Style& style) {
            style.background_color(theme.accent_color).border_color(theme.accent_color);
        });
    }
};

// gives the resizable example its own surface without making the layout container theme-aware.
class DemoResizableNodes final : public ui::ResizableContainer {
public:
    DemoResizableNodes(std::string id, const ui::Theme& theme) : ui::ResizableContainer(std::move(id)) {
        apply_theme_defaults(theme);
    }

protected:
    void apply_theme_defaults(const ui::Theme& theme) override {
        configure_all_styles([&theme](ui::Style& style) {
            style.background_color(theme.background_tertiary_color)
                .border(ui::BORDER_NONE)
                .border_radius(6.0F)
                .padding({20.0F, 20.0F})
                .cursor(ImGuiMouseCursor_ResizeNWSE);
        });
    }
};

// owns the list children so changing its values does not leak stale retained nodes.
class DemoTextListWidget final : public ui::StackContainer {
public:
    DemoTextListWidget(const ui::Theme& theme, std::vector<std::string> items);
    DemoTextListWidget& set_items(std::vector<std::string> items);

protected:
    void apply_theme_defaults(const ui::Theme& theme) override {
        configure_all_styles([&theme](ui::Style& style) {
            style.padding({8.0F, 8.0F})
                .background_color(theme.background_tertiary_color)
                .border(ui::BORDER_NONE)
                .border_radius(6.0F);
        });
    }
};

// keeps the demo controls and dynamic subtree under one retained root.
class DemoScreen final : public ui::StackContainer {
public:
    DemoScreen(UI& surface, std::string backend);
    void setup_dynamic_nodes(ui::Node& parent);
    int& blur();

private:
    void on_update(float dt) override;
    static void apply_border_style(ui::Node& node, ui::BorderStyle style);

protected:
    void apply_theme_defaults(const ui::Theme& theme) override {
        configure_all_styles([&theme](ui::Style& style) {
            style.padding({24.0F, 24.0F}).background_color(theme.background_secondary_color);
        });
    }

private:
    UI& m_surface;
    ui::ResizableContainer* m_dynamic_nodes = nullptr;
    ui::TextWidget* m_dynamic_status = nullptr;
    ui::Node* m_pending_remove = nullptr;
    bool m_enabled = true;
    int m_clicks = 0;
    DemoTextListWidget* m_text_list = nullptr;
    bool m_text_list_horizontal = false;
    int m_dynamic_count = 0;
    int m_next_dynamic_id = 0;
    std::string m_name = "imgui-ui";
    std::string m_theme = "blue";
    std::string m_border_style = "solid";
    int m_blur = 5;
};

DemoTextListWidget::DemoTextListWidget(const ui::Theme& theme, std::vector<std::string> items)
    : ui::StackContainer("demo-text-list") {
    // fit sizing follows the text, while spacing keeps each item readable.
    set_spacing(8.0F);
    set_size({ui::fit(), ui::fit()});
    apply_theme_defaults(theme);
    set_items(std::move(items));
}

DemoTextListWidget& DemoTextListWidget::set_items(std::vector<std::string> items) {
    // clearing first removes stale retained children before the new values are attached.
    clear();
    for (std::string& item : items) {
        add<ui::TextWidget>(std::move(item));
    }
    return *this;
}

DemoScreen::DemoScreen(UI& surface, std::string backend)
    : ui::StackContainer("demo", ui::StackDirection::Vertical), m_surface(surface) {
    // grow sizing lets the root own the surface box, while scrolling keeps every section reachable.
    set_size({ui::grow(), ui::grow()});
    set_scrollable(true);
    set_spacing(16.0F);
    apply_theme_defaults(surface.theme());

    // this section exposes runtime context without putting backend-specific behavior in widgets.
    auto& overview = add<DemoPanel>("overview", surface.theme());
    overview.set_size({ui::grow(), ui::fit()});
    overview.set_spacing(4.0F);
    overview.add<ui::TextWidget>("imgui-ui example");
    overview.add<ui::TextWidget>(std::format("backend: {}", backend));
    if (backend == "sdl") {
        overview.add<ui::TextWidget>("debugger: shift + d");
    }

    // these controls share one retained section so input, fonts, and transitions can be compared directly.
    auto& profile = add<DemoPanel>("profile", surface.theme());
    profile.set_size({ui::grow(), ui::fit()});
    profile.set_spacing(8.0F);
    profile.add<ui::TextWidget>("profile");
    auto& name_input = profile.add<ui::TextInputWidget>(surface, m_name, "name");
    name_input.set_size({ui::px(360.0F), ui::px(42.0F)});
    name_input.set_icon(m_surface.runtime().textures().find("demo-file-icon"));
    profile.add<ui::CheckboxWidget>(surface, m_enabled, "enabled").set_size({ui::px(360.0F), ui::px(32.0F)});

    // the hover slot changes line height and shadow, showing that visual transitions stay in node styles.
    auto& custom_line_text = profile.add<ui::TextWidget>("hover for shadow + custom line height");
    custom_line_text.set_input_target();
    custom_line_text.configure_all_styles([](ui::Style& style) {
        style.box_shadow({}, 0.15F);
        style.line_height(1.0F, 0.1F);
    });

    custom_line_text.configure_style(ui::StyleType::HOVER, [](ui::Style& style) {
        style.line_height(2.0F, 0.1F);
        style.color({0, 150, 255}, 0.1F);
        style.box_shadow(
            {
                .offset = {0.0F, 3.0F},
                .blur = 5.0F,
                .spread = 5.0F,
                .color = ImColor{1.0F, 1.0F, 1.0F, 0.5F},
            },
            0.15F
        );
    });

    // equal widths make ellipsis truncation and plain clipping visibly different.
    profile.add<ui::TextWidget>("ellipsis: this text is longer than the available width")
        .set_size({ui::px(220.0F), ui::px(20.0F)})
        .set_overflow(ui::TextOverflow::Ellipsis);
    profile.add<ui::TextWidget>("clip: this text is longer than the available width").set_size({ui::px(220.0F), ui::px(20.0F)});

    // these controls change visual state on the retained tree without rebuilding widgets.
    auto& appearance = add<DemoPanel>("appearance", surface.theme());
    appearance.set_size({ui::grow(), ui::fit()});
    appearance.set_spacing(8.0F);
    appearance.add<ui::TextWidget>("appearance");

    auto& theme = appearance.add<ui::DropdownWidget>(
        surface, m_theme, std::vector<ui::DropdownOption>{{"blue", "blue"}, {"deep dark", "dark"}}, "theme"
    );

    theme.set_label("theme").set_size({ui::px(360.0F), ui::px(68.0F)});
    // one palette update keeps backgrounds, controls, borders, text, and imgui colors in sync.
    theme.on_change = [this] { m_surface.set_theme(make_demo_theme(m_theme == "dark")); };

    auto& border_style = appearance.add<ui::DropdownWidget>(
        surface, m_border_style, std::vector<ui::DropdownOption>{{"solid", "solid"}, {"dashed", "dashed"}, {"dotted", "dotted"}},
        "border-style"
    );

    border_style.set_label("border style").set_size({ui::px(360.0F), ui::px(68.0F)});
    // borders live in each node style, so changing the option walks the retained tree once.
    border_style.on_change = [this] {
        const ui::BorderStyle style = m_border_style == "dashed"   ? ui::BorderStyle::Dashed
                                      : m_border_style == "dotted" ? ui::BorderStyle::Dotted
                                                                   : ui::BorderStyle::Solid;
        apply_border_style(m_surface.root(), style);
    };

    // the callback mutates an existing text node, demonstrating retained state without rebuilding its section.
    auto& actions = add<DemoPanel>("actions", surface.theme());
    actions.set_size({ui::grow(), ui::fit()});
    actions.set_spacing(8.0F);
    actions.add<ui::TextWidget>("actions");
    auto& status = actions.add<ui::TextWidget>("no clicks yet");
    auto& button = actions.add<ui::ButtonWidget>(surface, "click me", ui::LayoutSize{ui::px(140.0F), ui::px(44.0F)});

    // this section combines measured text with a stack whose direction and children can change at runtime.
    auto& list_section = add<DemoPanel>("list-section", surface.theme());
    list_section.set_size({ui::grow(), ui::fit()});
    list_section.set_spacing(8.0F);
    list_section.add<ui::TextWidget>("dynamic stack layout");
    m_text_list = &list_section.add<DemoTextListWidget>(
        surface.theme(), std::vector<std::string>{"first item", "second item", "third item"}
    );

    auto& text_list_orientation =
        list_section.add<ui::ButtonWidget>(surface, "list orientation: vertical", ui::LayoutSize{ui::px(240.0F), ui::px(36.0F)});
    text_list_orientation.on_click([this, &text_list_orientation] {
        m_text_list_horizontal = !m_text_list_horizontal;
        m_text_list->set_direction(m_text_list_horizontal ? ui::StackDirection::Horizontal : ui::StackDirection::Vertical);
        text_list_orientation.set_text(m_text_list_horizontal ? "list orientation: horizontal" : "list orientation: vertical");
    });

    button.on_click([this, &status] {
        ++m_clicks;
        status.set_text(std::format("button clicks: {}", m_clicks));
    });
}

void DemoScreen::setup_dynamic_nodes(ui::Node& parent) {
    // anchoring keeps dynamic content out of the main flow, so adding rows cannot move other sections.
    auto& dynamic_section = parent.add<DemoPanel>(
        "dynamic-section", m_surface.theme(), DemoPanelTone::Base, ImVec2{14.0F, 14.0F}, ui::BORDER_ALL, true, true
    );

    dynamic_section.set_direction(ui::StackDirection::Horizontal);
    dynamic_section.set_spacing(8.0F);
    dynamic_section.set_layout({
        .size = {ui::px(460.0F), ui::px(220.0F)},
        .placement = {.anchor = ui::Anchor::TopRight, .origin = ui::Origin::TopRight, .offset = {-20.0F, 72.0F}},
        .in_flow = false,
    });

    // a fixed control column keeps its actions stable while the list column resizes.
    auto& node_controls = dynamic_section.add<ui::StackContainer>("dynamic-node-controls", ui::StackDirection::Vertical);
    node_controls.set_size({ui::px(120.0F), ui::grow()});
    node_controls.set_spacing(8.0F);

    // grow sizing gives the list the space left by the fixed control column.
    auto& dynamic_list = dynamic_section.add<ui::StackContainer>("dynamic-list", ui::StackDirection::Vertical);
    dynamic_list.set_size({ui::px(300.0F), ui::grow()});
    dynamic_list.set_spacing(8.0F);
    m_dynamic_status = &dynamic_list.add<ui::TextWidget>("dynamic nodes: 0");
    dynamic_list.add<ui::TextWidget>("click a list item to remove it");

    // this container owns scrolling and resizing; its children only describe their own size.
    m_dynamic_nodes = &dynamic_list.add<DemoResizableNodes>("dynamic-nodes", m_surface.theme());
    m_dynamic_nodes->set_size({ui::px(240.0F), ui::grow()});
    m_dynamic_nodes->set_resize(ui::ResizeAxes::Both).set_spacing(8.0F).set_scrollable(true);

    // adding a child updates the retained subtree and the status text without rebuilding the section.
    auto& add_node = node_controls.add<ui::ButtonWidget>(m_surface, "add node", ui::LayoutSize{ui::px(120.0F), ui::px(36.0F)});
    add_node.on_click([this] {
        ++m_dynamic_count;

        const int item_id = ++m_next_dynamic_id;
        auto& item = m_dynamic_nodes->add<ui::ButtonWidget>(
            m_surface, std::format("list item {}", item_id), ui::LayoutSize{ui::grow(), ui::px(36.0F)}
        );
        ui::ButtonWidget* item_ptr = &item;
        item.on_click([this, item_ptr] { m_pending_remove = item_ptr; });

        m_dynamic_status->set_text(std::format("dynamic nodes: {}", m_dynamic_count));
    });

    // removing only the last child keeps the example's retained ownership visible.
    auto& remove_node =
        node_controls.add<ui::ButtonWidget>(m_surface, "remove node", ui::LayoutSize{ui::px(120.0F), ui::px(36.0F)});
    remove_node.on_click([this] {
        if (m_dynamic_nodes->children().empty()) {
            return;
        }

        m_pending_remove = nullptr;
        m_dynamic_nodes->remove(*m_dynamic_nodes->children().back());

        --m_dynamic_count;
        m_dynamic_status->set_text(std::format("dynamic nodes: {}", m_dynamic_count));
    });

    // clearing the container destroys every retained child and resets the visible count.
    auto& clear_nodes =
        node_controls.add<ui::ButtonWidget>(m_surface, "clear nodes", ui::LayoutSize{ui::px(120.0F), ui::px(36.0F)});
    clear_nodes.on_click([this] {
        m_pending_remove = nullptr;
        m_dynamic_nodes->clear();
        m_dynamic_count = 0;
        m_dynamic_status->set_text("dynamic nodes: 0");
    });
}

int& DemoScreen::blur() {
    return m_blur;
}

void DemoScreen::on_update(float) {
    // defer destruction until dispatch finishes because the click callback still references the item.
    if (m_pending_remove != nullptr) {
        ui::Node* pending_remove = m_pending_remove;
        m_pending_remove = nullptr;
        if (m_dynamic_nodes->contains(pending_remove)) {
            m_dynamic_nodes->remove(*pending_remove);
            --m_dynamic_count;
            m_dynamic_status->set_text(std::format("dynamic nodes: {}", m_dynamic_count));
        }
    }
}

void DemoScreen::apply_border_style(ui::Node& node, ui::BorderStyle style) {
    // styles belong to individual nodes, so the border change must visit every retained descendant.
    if (auto* styled_node = dynamic_cast<ui::StyledNode*>(&node)) {
        styled_node->configure_all_styles([style](ui::Style& current_style) { current_style.border_style(style); });
    }

    for (const auto& child : node.children()) {
        apply_border_style(*child, style);
    }
}

void setup_demo(UI& surface, std::string backend) {
    ui::Runtime& runtime = surface.runtime();
#ifdef IMGUI_UI_ASSETS_DIR
    // register paths before widgets request fonts because each size loads lazily for the active imgui context.
    const std::filesystem::path assets = std::filesystem::path{IMGUI_UI_ASSETS_DIR};
    runtime.fonts().add("Inter Regular", assets / "fonts/Inter.ttf");
    runtime.fonts().add("Inter SemiBold", assets / "fonts/Inter.ttf");
    runtime.fonts().add("Inter Bold", assets / "fonts/Inter.ttf");
    surface.set_primary_font(runtime.fonts().find("Inter Regular"));
    surface.set_secondary_font(runtime.fonts().find("Inter SemiBold"));

    // registering both sources demonstrates the path and string-view texture loading overloads.
    runtime.textures().add("demo-file-icon", assets / "icons/demo.svg");
    runtime.textures().add("demo-inline-icon", DEMO_INLINE_ICON_SVG);
#endif
    ui::Texture* inline_icon = runtime.textures().find("demo-inline-icon");

    // create normal content first so later layers render above it without changing its flow layout.
    auto& demo = surface.root().add<DemoScreen>(surface, std::move(backend));

    // an inline layer shares the surface window, so anchored children stay outside the main flow.
    auto& overlay = surface.root().add<ui::LayerContainer>("##demo-overlay", ui::LayerMode::Inline);
    auto& panel = overlay.add<DemoPanel>("overlay-panel", surface.theme());

    panel.set_layout({
        .size = {ui::fit(), ui::fit()},
        .placement =
            {.anchor = ui::Anchor::TopRight, .origin = ui::Origin::TopRight, .offset = {-(460.0F + 12.0F + 20.0F), 72.0F}},
        .in_flow = false,
    });
    // starting hidden demonstrates visibility changes without rebuilding or repositioning siblings.
    panel.add<ui::TextWidget>("this panel is on the overlay layer");
    panel.set_visible(false);

    demo.setup_dynamic_nodes(overlay);

    // keeping the toggle out of flow prevents showing the panel from moving normal content.
    auto& overlay_button = overlay.add<DemoAccentButton>(surface, "show overlay", ui::LayoutSize{ui::px(160.0F), ui::px(40.0F)});

    overlay_button.set_layout({
        .size = {ui::px(160.0F), ui::px(40.0F)},
        .placement = {.anchor = ui::Anchor::TopRight, .origin = ui::Origin::TopRight, .offset = {-20.0F, 20.0F}},
        .in_flow = false,
    });
    overlay_button.on_click([&overlay_button, &panel] {
        panel.set_visible(!panel.visible());
        overlay_button.set_text(panel.visible() ? "hide overlay" : "show overlay");
    });

    // the menu is retained by the root, while its submenu icon comes from inline texture data.
    auto& context_status = demo.add<ui::TextWidget>("context menu: no selection");
    auto& context_button =
        demo.add<ui::ButtonWidget>(surface, "open context menu", ui::LayoutSize{ui::px(220.0F), ui::px(40.0F)});
    ui::ContextMenuItems context_items = {
        ui::ContextMenuItem::action(
            "first action", [&context_status](auto&) { context_status.set_text("context menu: first action"); }
        ),
        ui::ContextMenuItem::submenu("more actions", {ui::ContextMenuItem::action("second action", [&context_status](auto&) {
                                         context_status.set_text("context menu: second action");
                                     })}),
    };

    auto& context_menu = surface.root().add<ui::ContextMenuWidget>(surface, std::move(context_items), inline_icon);
    context_menu.set_hover_close_delay(2.0f);

    context_button.on_click([&context_menu] { context_menu.show(); });

    // a sibling layer can block background input while leaving its panel's own controls interactive.
    auto& input_blocker = surface.root().add<ui::LayerContainer>("##input-blocker", ui::LayerMode::Inline);
    input_blocker.set_visible(false);

    auto& blocker_panel = input_blocker.add<DemoPanel>("input-blocker-panel", surface.theme());

    blocker_panel.set_layout({
        .size = {ui::px(320.0F), ui::px(150.0F)},
        .placement = {.anchor = ui::Anchor::Center, .origin = ui::Origin::Center},
        .in_flow = false,
    });
    blocker_panel.set_spacing(10.0F);
    blocker_panel.add<ui::TextWidget>("pointer input is blocked below this panel");

    auto& block_button =
        demo.add<ui::ButtonWidget>(surface, "block pointer input", ui::LayoutSize{ui::px(220.0F), ui::px(40.0F)});
    auto& unblock_button =
        blocker_panel.add<ui::ButtonWidget>(surface, "disable pointer block", ui::LayoutSize{ui::px(284.0F), ui::px(40.0F)});

    ui::LayerContainer* blocker_ptr = &input_blocker;
    ui::ButtonWidget* block_button_ptr = &block_button;
    block_button.on_click([blocker_ptr, block_button_ptr] {
        blocker_ptr->set_visible(true);
        // owner-scoped blocking leaves descendants eligible while consuming clicks outside the panel.
        blocker_ptr->set_input_blocker();
        block_button_ptr->set_text("pointer input blocked");
    });

    unblock_button.on_click([blocker_ptr, block_button_ptr] {
        blocker_ptr->clear_input();
        blocker_ptr->set_visible(false);
        block_button_ptr->set_text("block pointer input");
    });

    // a window layer samples the already-rendered app for backdrop blur and owns modal input blocking.
    auto& modal_layer = surface.root().add<ui::LayerContainer>("##modal-layer", ui::LayerMode::Window);
    modal_layer.set_visible(false);
    modal_layer.set_input_blocker();
    modal_layer.configure_all_styles([](ui::Style& style) { style.background_color(ImColor{0.0F, 0.0F, 0.0F, 0.0F}).blur(5); });

    auto& modal =
        modal_layer.add<DemoPanel>("demo-modal", surface.theme(), DemoPanelTone::Secondary, ImVec2{24.0F, 24.0F}, ui::BORDER_ALL);
    modal.set_visible(false);
    modal.set_layout({
        .size = {ui::px(480.0F), ui::px(220.0F)},
        .placement = {.anchor = ui::Anchor::Center, .origin = ui::Origin::Center},
        .in_flow = false,
    });
    modal.set_spacing(10.0F);
    // the layer handles backdrop and escape dismissal so the modal owns its close policy.
    modal_layer.on_event = [&modal_layer, &modal](ui::UiEvent& event) {
        const bool clicked_outside = (event.type == ui::EventType::Click || event.type == ui::EventType::PointerDown) &&
                                     event.button == ui::PointerButton::Left &&
                                     !modal.layout().visual_rect().contains(event.position);
        const bool pressed_escape =
            event.type == ui::EventType::Cancel || (event.type == ui::EventType::KeyDown && event.key == ui::Key::Escape);
        if (!clicked_outside && !pressed_escape) {
            return;
        }

        modal.set_visible(false);
        modal_layer.set_visible(false);
        event.stop_propagation();
    };

    modal.add<ui::TextWidget>("modal overlay");
    auto& blur = modal.add<ui::NumberInputWidget>(surface, demo.blur(), "modal-blur");
    blur.set_label("backdrop blur").set_range(0, 32).set_size({ui::px(180.0F), ui::px(48.0F)});
    blur.on_change = [&demo, &modal_layer] {
        modal_layer.configure_all_styles([&demo](ui::Style& style) { style.blur(demo.blur()); });
    };

    auto& close_button = modal.add<ui::ButtonWidget>(surface, "close modal", ui::LayoutSize{ui::px(180.0F), ui::px(40.0F)});
    close_button.on_click([&modal_layer, &modal] {
        modal.set_visible(false);
        modal_layer.set_visible(false);
    });

    auto& modal_button = demo.add<ui::ButtonWidget>(surface, "open modal", ui::LayoutSize{ui::px(220.0F), ui::px(40.0F)});
    modal_button.on_click([&modal_layer, &modal, &surface] {
        if (modal.visible()) {
            return;
        }

        modal_layer.set_visible(true);
        modal.set_visible(true);
        modal_layer.request_focus();
        surface.input_router().set_focus(modal_layer);
    });

    // text nodes do not receive a runtime in their constructors, so refresh the completed tree once.
    surface.set_theme(surface.theme());
}
