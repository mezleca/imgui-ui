#include "demo.hpp"

#include <ui/layout/layer-container.hpp>
#include <ui/layout/resizable-container.hpp>
#include <ui/style/style.hpp>
#include <ui/style/styled-node.hpp>
#include <ui/ui.hpp>
#include <ui/widgets/button.hpp>
#include <ui/widgets/checkbox.hpp>
#include <ui/widgets/context-menu.hpp>
#include <ui/widgets/dropdown.hpp>
#include <ui/widgets/number-input.hpp>
#include <ui/widgets/text-input.hpp>
#include <ui/widgets/text.hpp>

#include <format>
#include <utility>
#include <vector>

class DemoTextListWidget final : public ui::StackContainer {
public:
    explicit DemoTextListWidget(std::vector<std::string> items);
    DemoTextListWidget& set_items(std::vector<std::string> items);
};

class DemoScreen final : public ui::StackContainer {
public:
    DemoScreen(UI& surface, std::string backend);
    void setup_dynamic_nodes(ui::Node& parent);
    int& blur();

private:
    void on_update(float dt) override;
    static void apply_border_style(ui::Node& node, ui::BorderStyle style);

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

static void style_demo_panel(ui::Style& style, const ui::Theme& theme) {
    style.padding({14.0F, 14.0F})
        .background_color(theme.background_color)
        .border(ui::BORDER_NONE)
        .border_color(theme.border_color)
        .border_radius(6.0F);
}

DemoTextListWidget::DemoTextListWidget(std::vector<std::string> items) : ui::StackContainer("demo-text-list") {
    set_spacing(8.0F);
    set_size({ui::fit(), ui::fit()});
    set_items(std::move(items));
}

DemoTextListWidget& DemoTextListWidget::set_items(std::vector<std::string> items) {
    clear();
    for (std::string& item : items) {
        add<ui::TextWidget>(std::move(item));
    }
    return *this;
}

DemoScreen::DemoScreen(UI& surface, std::string backend)
    : ui::StackContainer("demo", ui::StackDirection::Vertical), m_surface(surface) {
    set_size({ui::grow(), ui::grow()});
    set_scrollable(true);
    set_spacing(16.0F);

    configure_all_styles([&surface](ui::Style& style) {
        style.padding({24.0F, 24.0F}).background_color(surface.theme().background_secondary_color);
    });

    auto& overview = add<ui::StackContainer>("overview");
    overview.set_size({ui::grow(), ui::fit()});
    overview.set_spacing(4.0F);
    overview.configure_all_styles([&surface](ui::Style& style) { style_demo_panel(style, surface.theme()); });
    overview.add<ui::TextWidget>("imgui-ui example");
    overview.add<ui::TextWidget>(std::format("backend: {}", backend));
    if (backend == "sdl") {
        overview.add<ui::TextWidget>("debugger: shift + d");
    }

    auto& profile = add<ui::StackContainer>("profile");
    profile.set_size({ui::grow(), ui::fit()});
    profile.set_spacing(8.0F);
    profile.configure_all_styles([&surface](ui::Style& style) { style_demo_panel(style, surface.theme()); });
    profile.add<ui::TextWidget>("profile");
    profile.add<ui::TextInputWidget>(surface, m_name, "name").set_size({ui::px(360.0F), ui::px(42.0F)});
    profile.add<ui::CheckboxWidget>(surface, m_enabled, "enabled").set_size({ui::px(360.0F), ui::px(32.0F)});

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

    profile.add<ui::TextWidget>("ellipsis: this text is longer than the available width")
        .set_size({ui::px(220.0F), ui::px(20.0F)})
        .set_overflow(ui::TextOverflow::Ellipsis);
    profile.add<ui::TextWidget>("clip: this text is longer than the available width").set_size({ui::px(220.0F), ui::px(20.0F)});

    auto& appearance = add<ui::StackContainer>("appearance");
    appearance.set_size({ui::grow(), ui::fit()});
    appearance.set_spacing(8.0F);
    appearance.configure_all_styles([&surface](ui::Style& style) { style_demo_panel(style, surface.theme()); });
    appearance.add<ui::TextWidget>("appearance");

    auto& theme = appearance.add<ui::DropdownWidget>(
        surface, m_theme, std::vector<ui::DropdownOption>{{"blue", "blue"}, {"high contrast", "contrast"}}, "theme"
    );

    theme.set_label("theme").set_size({ui::px(360.0F), ui::px(68.0F)});
    theme.on_change = [this] {
        const ImColor background =
            m_theme == "contrast" ? ImColor{0.0F, 0.0F, 0.0F, 1.0F} : ImColor{m_surface.theme().background_color};
        configure_all_styles([background](ui::Style& style) { style.background_color(background); });
    };

    auto& border_style = appearance.add<ui::DropdownWidget>(
        surface, m_border_style, std::vector<ui::DropdownOption>{{"solid", "solid"}, {"dashed", "dashed"}, {"dotted", "dotted"}},
        "border-style"
    );

    border_style.set_label("border style").set_size({ui::px(360.0F), ui::px(68.0F)});
    border_style.on_change = [this] {
        const ui::BorderStyle style = m_border_style == "dashed"   ? ui::BorderStyle::Dashed
                                      : m_border_style == "dotted" ? ui::BorderStyle::Dotted
                                                                   : ui::BorderStyle::Solid;
        apply_border_style(m_surface.root(), style);
    };

    auto& actions = add<ui::StackContainer>("actions");
    actions.set_size({ui::grow(), ui::fit()});
    actions.set_spacing(8.0F);
    actions.configure_all_styles([&surface](ui::Style& style) { style_demo_panel(style, surface.theme()); });
    actions.add<ui::TextWidget>("actions");
    auto& status = actions.add<ui::TextWidget>("no clicks yet");
    auto& button = actions.add<ui::ButtonWidget>(surface, "click me", ui::LayoutSize{ui::px(140.0F), ui::px(44.0F)});

    auto& list_section = add<ui::StackContainer>("list-section");
    list_section.set_size({ui::grow(), ui::fit()});
    list_section.set_spacing(8.0F);
    list_section.configure_all_styles([&surface](ui::Style& style) { style_demo_panel(style, surface.theme()); });
    list_section.add<ui::TextWidget>("dynamic stack layout");
    m_text_list = &list_section.add<DemoTextListWidget>(std::vector<std::string>{"first item", "second item", "third item"});
    m_text_list->configure_all_styles([&surface](ui::Style& style) {
        style.padding({8.0F, 8.0F})
            .background_color(surface.theme().background_tertiary_color)
            .border(ui::BORDER_NONE)
            .border_radius(6.0F);
    });

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
    auto& dynamic_section = parent.add<ui::StackContainer>("dynamic-section", ui::StackDirection::Horizontal);

    dynamic_section.set_spacing(8.0F);
    dynamic_section.set_layout({
        .size = {ui::px(460.0F), ui::px(220.0F)},
        .placement = {.anchor = ui::Anchor::TopRight, .origin = ui::Origin::TopRight, .offset = {-20.0F, 72.0F}},
        .in_flow = false,
    });

    dynamic_section.configure_all_styles([this](ui::Style& style) {
        style_demo_panel(style, m_surface.theme());
        style.border_color(m_surface.theme().accent_color).border(ui::BORDER_ALL);
        style.box_shadow({
            .offset = {0.0F, 8.0F},
            .blur = 18.0F,
            .spread = 2.0F,
            .color = ImColor{0.0F, 0.0F, 0.0F, 0.45F},
        });
    });

    auto& node_controls = dynamic_section.add<ui::StackContainer>("dynamic-node-controls", ui::StackDirection::Vertical);
    node_controls.set_size({ui::px(120.0F), ui::grow()});
    node_controls.set_spacing(8.0F);

    auto& dynamic_list = dynamic_section.add<ui::StackContainer>("dynamic-list", ui::StackDirection::Vertical);
    dynamic_list.set_size({ui::px(300.0F), ui::grow()});
    dynamic_list.set_spacing(8.0F);
    m_dynamic_status = &dynamic_list.add<ui::TextWidget>("dynamic nodes: 0");
    dynamic_list.add<ui::TextWidget>("click a list item to remove it");

    m_dynamic_nodes = &dynamic_list.add<ui::ResizableContainer>("dynamic-nodes");
    m_dynamic_nodes->set_size({ui::px(240.0F), ui::grow()});
    m_dynamic_nodes->set_resize(ui::ResizeAxes::Both).set_spacing(8.0F).set_scrollable(true);
    m_dynamic_nodes->configure_all_styles([this](ui::Style& style) {
        style.background_color(m_surface.theme().background_tertiary_color)
            .border(ui::BORDER_NONE)
            .border_radius(6.0F)
            .padding({20.0F, 20.0F})
            .cursor(ImGuiMouseCursor_ResizeNWSE);
    });

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
    // defer destruction until the item's event has finished dispatching.
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
    if (auto* styled_node = dynamic_cast<ui::StyledNode*>(&node)) {
        styled_node->configure_all_styles([style](ui::Style& current_style) { current_style.border_style(style); });
    }

    for (const auto& child : node.children()) {
        apply_border_style(*child, style);
    }
}

void setup_demo(UI& surface, std::string backend) {
    auto& demo = surface.root().add<DemoScreen>(surface, std::move(backend));
    auto& overlay = surface.root().add<ui::LayerContainer>("##demo-overlay", ui::LayerMode::Inline);
    auto& panel = overlay.add<ui::StackContainer>("overlay-panel");

    panel.configure_all_styles([&surface](ui::Style& style) { style_demo_panel(style, surface.theme()); });

    panel.set_layout({
        .size = {ui::fit(), ui::fit()},
        .placement =
            {.anchor = ui::Anchor::TopRight, .origin = ui::Origin::TopRight, .offset = {-(460.0F + 12.0F + 20.0F), 72.0F}},
        .in_flow = false,
    });
    panel.add<ui::TextWidget>("this panel is on the overlay layer");
    panel.set_visible(false);

    demo.setup_dynamic_nodes(overlay);

    auto& overlay_button = overlay.add<ui::ButtonWidget>(surface, "show overlay", ui::LayoutSize{ui::px(160.0F), ui::px(40.0F)});

    overlay_button.set_layout({
        .size = {ui::px(160.0F), ui::px(40.0F)},
        .placement = {.anchor = ui::Anchor::TopRight, .origin = ui::Origin::TopRight, .offset = {-20.0F, 20.0F}},
        .in_flow = false,
    });
    overlay_button.configure_all_styles([&surface](ui::Style& style) {
        style.background_color(surface.theme().accent_color).border_color(surface.theme().accent_hover_color);
    });

    overlay_button.on_click([&overlay_button, &panel] {
        panel.set_visible(!panel.visible());
        overlay_button.set_text(panel.visible() ? "hide overlay" : "show overlay");
    });

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

    auto& context_menu = surface.root().add<ui::ContextMenuWidget>(surface, std::move(context_items));
    context_menu.set_hover_close_delay(2.0f);

    context_button.on_click([&context_menu] { context_menu.show(); });

    auto& input_blocker = surface.root().add<ui::LayerContainer>("##input-blocker", ui::LayerMode::Inline);
    input_blocker.set_visible(false);

    auto& blocker_panel = input_blocker.add<ui::StackContainer>("input-blocker-panel", ui::StackDirection::Vertical);

    blocker_panel.set_layout({
        .size = {ui::px(320.0F), ui::px(150.0F)},
        .placement = {.anchor = ui::Anchor::Center, .origin = ui::Origin::Center},
        .in_flow = false,
    });
    blocker_panel.set_spacing(10.0F);
    blocker_panel.configure_all_styles([&surface](ui::Style& style) { style_demo_panel(style, surface.theme()); });

    blocker_panel.add<ui::TextWidget>("pointer input is blocked below this panel");

    auto& block_button =
        demo.add<ui::ButtonWidget>(surface, "block pointer input", ui::LayoutSize{ui::px(220.0F), ui::px(40.0F)});
    auto& unblock_button =
        blocker_panel.add<ui::ButtonWidget>(surface, "disable pointer block", ui::LayoutSize{ui::px(284.0F), ui::px(40.0F)});

    ui::LayerContainer* blocker_ptr = &input_blocker;
    ui::ButtonWidget* block_button_ptr = &block_button;
    block_button.on_click([blocker_ptr, block_button_ptr] {
        blocker_ptr->set_visible(true);
        // blocks clicks outside blocker_panel without blocking its buttons.
        blocker_ptr->set_input_blocker();
        block_button_ptr->set_text("pointer input blocked");
    });

    unblock_button.on_click([blocker_ptr, block_button_ptr] {
        blocker_ptr->clear_input();
        blocker_ptr->set_visible(false);
        block_button_ptr->set_text("block pointer input");
    });

    auto& modal_layer = surface.root().add<ui::LayerContainer>("##modal-layer", ui::LayerMode::Window);
    modal_layer.set_visible(false);
    modal_layer.set_input_blocker();
    modal_layer.configure_all_styles([](ui::Style& style) { style.background_color(ImColor{0.0F, 0.0F, 0.0F, 0.0F}).blur(5); });

    auto& modal = modal_layer.add<ui::StackContainer>("demo-modal");
    modal.set_visible(false);
    modal.set_layout({
        .size = {ui::px(480.0F), ui::px(220.0F)},
        .placement = {.anchor = ui::Anchor::Center, .origin = ui::Origin::Center},
        .in_flow = false,
    });
    modal.set_spacing(10.0F);
    modal.configure_all_styles([&surface](ui::Style& style) {
        style.padding({24.0F, 24.0F})
            .background_color(surface.theme().background_secondary_color)
            .border(ui::BORDER_ALL)
            .border_color(surface.theme().border_color)
            .border_radius(8.0F);
    });

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
}
