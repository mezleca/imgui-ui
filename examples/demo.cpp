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
    if (backend == "sdl") add_child<ui::TextWidget>("debugger: shift + d");
    add_child<ui::TextInputWidget>(surface, m_name, "name").set_size({360.0F, 42.0F});
    add_child<ui::CheckboxWidget>(surface, m_enabled, "checkbox");
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

void DemoScreen::on_update(float) {
    if (m_applied_theme == m_theme) return;

    m_applied_theme = m_theme;
    const ImColor background =
        m_theme == "contrast" ? ImColor{0.0F, 0.0F, 0.0F, 1.0F} : ImColor{m_surface.theme().background_color};
    configure_all_styles([background](ui::Style& style) { style.background_color(background); });
}

void setup_demo(UI& surface, std::string backend) {
    surface.root().add_child<DemoScreen>(surface, std::move(backend));

    auto& overlay = surface.root().add_child<ui::OverlayNode>("##demo-overlay");
    overlay.set_input_layer(ui::InputLayer::Overlay);

    auto& panel = overlay.add_child<ui::StackContainer>("overlay-panel");
    panel.fit_content();
    panel.set_placement(ui::Anchor::TopRight, ui::Origin::TopRight, {-20.0F, 72.0F});
    panel.configure_all_styles([&surface](ui::Style& style) {
        style.padding({14.0F, 14.0F})
            .background_color(surface.theme().background_secondary_color)
            .border_color(surface.theme().accent_color)
            .border(ui::BORDER_ALL);
    });
    panel.add_child<ui::TextWidget>("this panel is on the overlay layer");
    panel.set_visible(false);

    auto& overlay_button = overlay.add_child<ui::ButtonWidget>(surface, "show overlay", ImVec2{160.0F, 40.0F});
    overlay_button.set_placement(ui::Anchor::TopRight, ui::Origin::TopRight, {-20.0F, 20.0F});
    overlay_button.configure_all_styles([&surface](ui::Style& style) {
        style.background_color(surface.theme().accent_color).border_color(surface.theme().accent_hover_color);
    });
    overlay_button.on_event = [&overlay_button, &panel](ui::UiEvent& event) {
        if (event.type != ui::EventType::Click) return;
        panel.set_visible(!panel.visible());
        overlay_button.try_set_content(panel.visible() ? "hide overlay" : "show overlay");
    };
}
