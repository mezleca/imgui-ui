#include "demo.hpp"

#include <ui/layout/overlay-container.hpp>
#include <ui/layout/modal-container.hpp>
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

static void style_demo_panel(ui::Style& style, const ui::Theme& theme) {
    style.padding({14.0F, 14.0F}).background_color(theme.background_secondary_color).border(ui::BORDER_NONE).border_radius(8.0F);
}

DemoTextListWidget::DemoTextListWidget(std::vector<std::string> items) : ui::StackContainer("demo-text-list") {
    set_spacing(8.0F);
    fit_content();
    set_items(std::move(items));
}

DemoTextListWidget& DemoTextListWidget::set_items(std::vector<std::string> items) {
    clear();
    for (std::string& item : items) {
        add_child<ui::TextWidget>(std::move(item));
    }
    return *this;
}

DemoScreen::DemoScreen(UI& surface, std::string backend)
    : ui::StackContainer("demo", ui::StackDirection::Vertical), m_surface(surface) {
    set_size({0.0F, 0.0F});
    set_scrollable(true);
    set_spacing(16.0F);

    configure_all_styles([&surface](ui::Style& style) {
        style.padding({24.0F, 24.0F}).background_color(surface.theme().background_color);
    });

    auto& overview = add_child<ui::StackContainer>("overview");
    overview.fit_content_height().set_spacing(4.0F);
    overview.configure_all_styles([&surface](ui::Style& style) { style_demo_panel(style, surface.theme()); });
    overview.add_child<ui::TextWidget>("imgui-ui example");
    overview.add_child<ui::TextWidget>(std::format("backend: {}", backend));
    if (backend == "sdl") {
        overview.add_child<ui::TextWidget>("debugger: shift + d");
    }

    auto& profile = add_child<ui::StackContainer>("profile");
    profile.fit_content_height().set_spacing(8.0F);
    profile.configure_all_styles([&surface](ui::Style& style) { style_demo_panel(style, surface.theme()); });
    profile.add_child<ui::TextWidget>("profile");
    profile.add_child<ui::TextInputWidget>(surface, m_name, "name").set_size({360.0F, 42.0F});
    profile.add_child<ui::CheckboxWidget>(surface, m_enabled, "enabled").set_size({360.0F, 32.0F});

    auto& custom_line_text = profile.add_child<ui::TextWidget>("hover this text to change its line height");
    custom_line_text.set_input_target();
    custom_line_text.configure_all_styles([](ui::Style& style) { style.line_height(1.0F, 0.1F); });
    custom_line_text.configure_style(ui::StyleType::HOVER, [](ui::Style& style) {
        style.line_height(2.0F, 0.1F);
        style.color({0, 150, 200}, 0.1F);
    });

    profile.add_child<ui::TextWidget>("ellipsis: this text is longer than the available width")
        .set_size({220.0F, 20.0F})
        .set_overflow(ui::TextOverflow::Ellipsis);
    profile.add_child<ui::TextWidget>("clip: this text is longer than the available width").set_size({220.0F, 20.0F});

    auto& appearance = add_child<ui::StackContainer>("appearance");
    appearance.fit_content_height().set_spacing(8.0F);
    appearance.configure_all_styles([&surface](ui::Style& style) { style_demo_panel(style, surface.theme()); });
    appearance.add_child<ui::TextWidget>("appearance");

    auto& theme = appearance.add_child<ui::DropdownWidget>(
        surface, m_theme, std::vector<ui::DropdownOption>{{"blue", "blue"}, {"high contrast", "contrast"}}, "theme"
    );

    theme.set_label("theme").set_size({360.0F, 68.0F});
    theme.on_change = [this] {
        const ImColor background =
            m_theme == "contrast" ? ImColor{0.0F, 0.0F, 0.0F, 1.0F} : ImColor{m_surface.theme().background_color};
        configure_all_styles([background](ui::Style& style) { style.background_color(background); });
    };

    auto& border_style = appearance.add_child<ui::DropdownWidget>(
        surface, m_border_style, std::vector<ui::DropdownOption>{{"solid", "solid"}, {"dashed", "dashed"}, {"dotted", "dotted"}},
        "border-style"
    );

    border_style.set_label("border style").set_size({360.0F, 68.0F});
    border_style.on_change = [this] {
        const ui::BorderStyle style = m_border_style == "dashed"   ? ui::BorderStyle::Dashed
                                      : m_border_style == "dotted" ? ui::BorderStyle::Dotted
                                                                   : ui::BorderStyle::Solid;
        apply_border_style(m_surface.root(), style);
    };

    auto& actions = add_child<ui::StackContainer>("actions");
    actions.fit_content_height().set_spacing(8.0F);
    actions.configure_all_styles([&surface](ui::Style& style) { style_demo_panel(style, surface.theme()); });
    actions.add_child<ui::TextWidget>("actions");
    auto& status = actions.add_child<ui::TextWidget>("no clicks yet");
    auto& button = actions.add_child<ui::ButtonWidget>(surface, "click me", ImVec2{140.0F, 44.0F});

    auto& list_section = add_child<ui::StackContainer>("list-section");
    list_section.fit_content_height().set_spacing(8.0F);
    list_section.configure_all_styles([&surface](ui::Style& style) { style_demo_panel(style, surface.theme()); });
    list_section.add_child<ui::TextWidget>("dynamic stack layout");
    m_text_list =
        &list_section.add_child<DemoTextListWidget>(std::vector<std::string>{"first item", "second item", "third item"});
    m_text_list->configure_all_styles([&surface](ui::Style& style) {
        style.padding({8.0F, 8.0F})
            .background_color(surface.theme().background_tertiary_color)
            .border(ui::BORDER_NONE)
            .border_radius(6.0F);
    });

    auto& text_list_orientation =
        list_section.add_child<ui::ButtonWidget>(surface, "list orientation: vertical", ImVec2{240.0F, 36.0F});
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
    auto& dynamic_section = parent.add_child<ui::StackContainer>("dynamic-section", ui::StackDirection::Horizontal);

    dynamic_section.set_spacing(8.0F)
        .set_size({460.0F, 220.0F})
        .set_placement({.anchor = ui::Anchor::TopRight, .origin = ui::Origin::TopRight, .offset = {-20.0F, 72.0F}});

    dynamic_section.configure_all_styles([this](ui::Style& style) {
        style_demo_panel(style, m_surface.theme());
        style.border_color(m_surface.theme().accent_color).border(ui::BORDER_ALL);
    });

    auto& node_controls = dynamic_section.add_child<ui::StackContainer>("dynamic-node-controls", ui::StackDirection::Vertical);
    node_controls.set_size({120.0F, 0.0F});
    node_controls.set_spacing(8.0F);

    auto& dynamic_list = dynamic_section.add_child<ui::StackContainer>("dynamic-list", ui::StackDirection::Vertical);
    dynamic_list.set_size({300.0F, 0.0F});
    dynamic_list.set_spacing(8.0F);
    m_dynamic_status = &dynamic_list.add_child<ui::TextWidget>("dynamic nodes: 0");
    dynamic_list.add_child<ui::TextWidget>("click a list item to remove it");

    m_dynamic_nodes = &dynamic_list.add_child<ui::ResizableContainer>("dynamic-nodes");
    m_dynamic_nodes->set_size({240.0F, 0.0F});
    m_dynamic_nodes->set_resize(ui::ResizeAxes::Both).set_spacing(8.0F).set_scrollable(true);
    m_dynamic_nodes->configure_all_styles([this](ui::Style& style) {
        style.background_color(m_surface.theme().background_tertiary_color)
            .border(ui::BORDER_NONE)
            .border_radius(6.0F)
            .padding({20.0F, 20.0F})
            .cursor(ImGuiMouseCursor_ResizeNWSE);
    });

    auto& add_node = node_controls.add_child<ui::ButtonWidget>(m_surface, "add node", ImVec2{120.0F, 36.0F});

    add_node.on_click([this] {
        ++m_dynamic_count;

        const int item_id = ++m_next_dynamic_id;
        auto& item =
            m_dynamic_nodes->add_child<ui::ButtonWidget>(m_surface, std::format("list item {}", item_id), ImVec2{0.0F, 36.0F});
        ui::ButtonWidget* item_ptr = &item;
        item.on_click([this, item_ptr] { m_pending_remove = item_ptr; });

        m_dynamic_status->set_text(std::format("dynamic nodes: {}", m_dynamic_count));
    });

    auto& remove_node = node_controls.add_child<ui::ButtonWidget>(m_surface, "remove node", ImVec2{120.0F, 36.0F});
    remove_node.on_click([this] {
        if (m_dynamic_nodes->children().empty()) {
            return;
        }

        m_pending_remove = nullptr;
        m_dynamic_nodes->remove(*m_dynamic_nodes->children().back());

        --m_dynamic_count;
        m_dynamic_status->set_text(std::format("dynamic nodes: {}", m_dynamic_count));
    });

    auto& clear_nodes = node_controls.add_child<ui::ButtonWidget>(m_surface, "clear nodes", ImVec2{120.0F, 36.0F});
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
    auto& demo = surface.root().add_child<DemoScreen>(surface, std::move(backend));
    auto& overlay = surface.root().add_child<ui::OverlayNode>("##demo-overlay");
    auto& panel = overlay.add_child<ui::StackContainer>("overlay-panel");

    panel.configure_all_styles([&surface](ui::Style& style) { style_demo_panel(style, surface.theme()); });

    panel.fit_content();
    panel.set_placement(
        {.anchor = ui::Anchor::TopRight, .origin = ui::Origin::TopRight, .offset = {-(460.0F + 12.0F + 20.0F), 72.0F}}
    );
    panel.add_child<ui::TextWidget>("this panel is on the overlay layer");
    panel.set_visible(false);

    demo.setup_dynamic_nodes(overlay);

    auto& overlay_button = overlay.add_child<ui::ButtonWidget>(surface, "show overlay", ImVec2{160.0F, 40.0F});

    overlay_button.set_placement({.anchor = ui::Anchor::TopRight, .origin = ui::Origin::TopRight, .offset = {-20.0F, 20.0F}});
    overlay_button.configure_all_styles([&surface](ui::Style& style) {
        style.background_color(surface.theme().accent_color).border_color(surface.theme().accent_hover_color);
    });

    overlay_button.on_click([&overlay_button, &panel] {
        panel.set_visible(!panel.visible());
        overlay_button.set_text(panel.visible() ? "hide overlay" : "show overlay");
    });

    auto& context_status = demo.add_child<ui::TextWidget>("context menu: no selection");
    auto& context_button = demo.add_child<ui::ButtonWidget>(surface, "open context menu", ImVec2{220.0F, 40.0F});
    ui::ContextMenuItems context_items = {
        ui::ContextMenuItem::action(
            "first action", [&context_status](auto&) { context_status.set_text("context menu: first action"); }
        ),
        ui::ContextMenuItem::submenu("more actions", {ui::ContextMenuItem::action("second action", [&context_status](auto&) {
                                         context_status.set_text("context menu: second action");
                                     })}),
    };

    auto& context_menu = surface.root().add_child<ui::ContextMenuWidget>(surface, std::move(context_items));
    context_menu.set_hover_close_delay(2.0f);

    context_button.on_click([&context_menu] { context_menu.show(); });

    auto& input_blocker = surface.root().add_child<ui::OverlayNode>("##input-blocker");
    input_blocker.set_visible(false);

    auto& blocker_panel = input_blocker.add_child<ui::StackContainer>("input-blocker-panel", ui::StackDirection::Vertical);

    blocker_panel.set_size({320.0F, 150.0F});
    blocker_panel.set_placement({.anchor = ui::Anchor::Center, .origin = ui::Origin::Center});
    blocker_panel.set_spacing(10.0F);
    blocker_panel.configure_all_styles([&surface](ui::Style& style) { style_demo_panel(style, surface.theme()); });

    blocker_panel.add_child<ui::TextWidget>("pointer input is blocked below this panel");

    auto& block_button = demo.add_child<ui::ButtonWidget>(surface, "block pointer input", ImVec2{220.0F, 40.0F});
    auto& unblock_button = blocker_panel.add_child<ui::ButtonWidget>(surface, "disable pointer block", ImVec2{284.0F, 40.0F});

    ui::OverlayNode* blocker_ptr = &input_blocker;
    ui::ButtonWidget* block_button_ptr = &block_button;
    block_button.on_click([blocker_ptr, block_button_ptr] {
        blocker_ptr->set_visible(true);
        // blocks clicks outside blocker_panel without blocking its buttons.
        blocker_ptr->set_blocks_pointer_input(true);
        block_button_ptr->set_text("pointer input blocked");
    });

    unblock_button.on_click([blocker_ptr, block_button_ptr] {
        blocker_ptr->set_blocks_pointer_input(false);
        blocker_ptr->set_visible(false);
        block_button_ptr->set_text("block pointer input");
    });

    auto& modals = surface.root().add_child<ui::ModalContainer>(surface);
    modals.configure_all_styles([](ui::Style& style) { style.background_color(ImColor{0.0F, 0.0F, 0.0F, 0.0F}).blur(5); });
    auto& modal_button = demo.add_child<ui::ButtonWidget>(surface, "open modal", ImVec2{220.0F, 40.0F});
    modal_button.on_click([&demo, &modals, &surface] {
        if (modals.has_open_modal()) {
            return;
        }

        auto& modal = modals.open("demo-modal");
        modal.add_child<ui::TextWidget>("modal overlay");
        auto& blur = modal.add_child<ui::NumberInputWidget>(surface, demo.blur(), "modal-blur");
        blur.set_label("backdrop blur").set_range(0, 32).set_size({180.0F, 48.0F});
        blur.on_change = [&demo, &modals] {
            modals.configure_all_styles([&demo](ui::Style& style) { style.blur(demo.blur()); });
        };

        auto& close_button = modal.add_child<ui::ButtonWidget>(surface, "close modal", ImVec2{180.0F, 40.0F});
        close_button.on_click([&modals, modal_ptr = &modal] { modals.close(*modal_ptr); });
    });
}
