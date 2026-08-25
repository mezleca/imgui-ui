#include "demo.hpp"

#include <ui/layout/overlay-container.hpp>
#include <ui/style/style.hpp>
#include <ui/ui.hpp>
#include <ui/widgets/button.hpp>
#include <ui/widgets/checkbox.hpp>
#include <ui/widgets/dropdown.hpp>
#include <ui/widgets/text-input.hpp>
#include <ui/widgets/text.hpp>

#include <format>
#include <utility>
#include <vector>

DemoScreen::DemoScreen(UI& surface, std::string backend)
    : ui::StackContainer("demo", ui::StackDirection::Vertical), m_surface(surface) {
    set_size({0.0F, 0.0F});
    set_spacing(16.0F);

    configure_all_styles([&surface](ui::Style& style) {
        style.padding({24.0F, 24.0F}).background_color(surface.theme().background_color);
    });

    add_child<ui::TextWidget>("imgui-ui example");
    add_child<ui::TextWidget>(std::format("backend: {}", backend));

    // raylib does not support shared contexts :C
    if (backend == "sdl") {
        add_child<ui::TextWidget>("debugger: shift + d");
    }

    add_child<ui::TextInputWidget>(surface, m_name, "name").set_size({360.0F, 42.0F});
    add_child<ui::CheckboxWidget>(surface, m_enabled, "checkbox").set_size({360.0F, 32.0F});

    add_child<ui::DropdownWidget>(
        surface, m_theme, std::vector<ui::DropdownOption>{{"blue", "blue"}, {"high contrast", "contrast"}}, "theme"
    )
        .set_label("theme")
        .set_size({360.0F, 68.0F});

    auto& status = add_child<ui::TextWidget>("no clicks yet");
    auto& button = add_child<ui::ButtonWidget>(surface, "click me", ImVec2{140.0F, 44.0F});

    button.on_event = [this, &status](ui::UiEvent& event) {
        if (event.type != ui::EventType::Click) return;
        ++m_clicks;
        status.try_set_content(std::format("button clicks: {}", m_clicks));
    };
}

void DemoScreen::setup_dynamic_nodes(ui::Node& parent) {
    auto& dynamic_section = parent.add_child<ui::StackContainer>("dynamic-section", ui::StackDirection::Horizontal);
    dynamic_section.set_size({460.0F, 220.0F});
    dynamic_section.set_placement(ui::Anchor::TopRight, ui::Origin::TopRight, {-20.0F, 72.0F});
    dynamic_section.set_spacing(8.0F);
    dynamic_section.configure_all_styles([this](ui::Style& style) {
        style.padding({12.0F, 12.0F})
            .background_color(m_surface.theme().background_secondary_color)
            .border_color(m_surface.theme().accent_color)
            .border(ui::BORDER_ALL);
    });

    auto& dynamic_list = dynamic_section.add_child<ui::StackContainer>("dynamic-list", ui::StackDirection::Vertical);
    dynamic_list.set_size({300.0F, 0.0F});
    dynamic_list.set_spacing(8.0F);
    m_dynamic_status = &dynamic_list.add_child<ui::TextWidget>("dynamic nodes: 0");
    dynamic_list.add_child<ui::TextWidget>("click a list item to remove it");

    m_dynamic_nodes = &dynamic_list.add_child<ui::StackContainer>("dynamic-nodes", ui::StackDirection::Vertical);
    m_dynamic_nodes->set_size({300.0F, 0.0F});
    m_dynamic_nodes->set_spacing(8.0F);
    m_dynamic_nodes->set_scrollable(true);

    auto& node_controls = dynamic_section.add_child<ui::StackContainer>("dynamic-node-controls", ui::StackDirection::Vertical);
    node_controls.set_size({120.0F, 0.0F});
    node_controls.set_spacing(8.0F);
    auto& add_node = node_controls.add_child<ui::ButtonWidget>(m_surface, "add node", ImVec2{120.0F, 36.0F});

    add_node.on_event = [this](ui::UiEvent& event) {
        if (event.type != ui::EventType::Click) {
            return;
        }

        ++m_dynamic_count;

        const int item_id = ++m_next_dynamic_id;
        auto& item =
            m_dynamic_nodes->add_child<ui::ButtonWidget>(m_surface, std::format("list item {}", item_id), ImVec2{300.0F, 36.0F});
        ui::ButtonWidget* item_ptr = &item;
        item.on_event = [this, item_ptr](ui::UiEvent& item_event) {
            if (item_event.type == ui::EventType::Click) {
                m_pending_remove = item_ptr;
            }
        };

        m_dynamic_status->try_set_content(std::format("dynamic nodes: {}", m_dynamic_count));
    };

    auto& remove_node = node_controls.add_child<ui::ButtonWidget>(m_surface, "remove node", ImVec2{120.0F, 36.0F});
    remove_node.on_event = [this](ui::UiEvent& event) {
        if (event.type != ui::EventType::Click || m_dynamic_nodes->children().empty()) {
            return;
        }

        m_pending_remove = nullptr;
        m_dynamic_nodes->remove(*m_dynamic_nodes->children().back());

        --m_dynamic_count;
        m_dynamic_status->try_set_content(std::format("dynamic nodes: {}", m_dynamic_count));
    };

    auto& clear_nodes = node_controls.add_child<ui::ButtonWidget>(m_surface, "clear nodes", ImVec2{120.0F, 36.0F});
    clear_nodes.on_event = [this](ui::UiEvent& event) {
        if (event.type != ui::EventType::Click) return;
        m_pending_remove = nullptr;
        m_dynamic_nodes->clear();
        m_dynamic_count = 0;
        m_dynamic_status->try_set_content("dynamic nodes: 0");
    };
}

void DemoScreen::on_update(float) {
    // defer destruction until the item's event has finished dispatching.
    if (m_pending_remove != nullptr) {
        ui::Node* pending_remove = m_pending_remove;
        m_pending_remove = nullptr;
        if (m_dynamic_nodes->contains(pending_remove)) {
            m_dynamic_nodes->remove(*pending_remove);
            --m_dynamic_count;
            m_dynamic_status->try_set_content(std::format("dynamic nodes: {}", m_dynamic_count));
        }
    }

    if (m_applied_theme == m_theme) return;

    m_applied_theme = m_theme;
    const ImColor background =
        m_theme == "contrast" ? ImColor{0.0F, 0.0F, 0.0F, 1.0F} : ImColor{m_surface.theme().background_color};
    configure_all_styles([background](ui::Style& style) { style.background_color(background); });
}

void setup_demo(UI& surface, std::string backend) {
    auto& demo = surface.root().add_child<DemoScreen>(surface, std::move(backend));

    auto& overlay = surface.root().add_child<ui::OverlayNode>("##demo-overlay");
    overlay.set_input_layer(ui::InputLayer::Overlay);

    auto& panel = overlay.add_child<ui::StackContainer>("overlay-panel");

    panel.configure_all_styles([&surface](ui::Style& style) {
        style.padding({14.0F, 14.0F})
            .background_color(surface.theme().background_secondary_color)
            .border_color(surface.theme().accent_color)
            .border(ui::BORDER_ALL);
    });

    panel.fit_content();
    panel.set_placement(ui::Anchor::TopRight, ui::Origin::TopRight, {-(460.0F + 12.0F + 20.0F), 72.0F});
    panel.add_child<ui::TextWidget>("this panel is on the overlay layer");
    panel.set_visible(false);

    demo.setup_dynamic_nodes(overlay);

    auto& overlay_button = overlay.add_child<ui::ButtonWidget>(surface, "show overlay", ImVec2{160.0F, 40.0F});

    overlay_button.set_placement(ui::Anchor::TopRight, ui::Origin::TopRight, {-20.0F, 20.0F});
    overlay_button.configure_all_styles([&surface](ui::Style& style) {
        style.background_color(surface.theme().accent_color).border_color(surface.theme().accent_hover_color);
    });

    overlay_button.on_event = [&overlay_button, &panel](ui::UiEvent& event) {
        if (event.type != ui::EventType::Click) {
            return;
        }

        panel.set_visible(!panel.visible());
        overlay_button.try_set_content(panel.visible() ? "hide overlay" : "show overlay");
    };
}
