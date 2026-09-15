#include "text-input.hpp"

#include "../style/theme.hpp"
#include "../ui.hpp"
#include "image.hpp"
#include "text.hpp"

#include <cfloat>
#include <imgui_stdlib.h>

using namespace ui;

class TextInputWidget::FieldNode final : public StyledNode {
public:
    FieldNode(std::string& value, bool& focus_requested)
        : StyledNode("text", "TextInput"), m_value(&value), m_focus_requested(&focus_requested) {}

    bool accepts_input() const override {
        return false;
    }

    bool changed() const {
        return m_changed;
    }

private:
    bool paint() override {
        if (*m_focus_requested) {
            ImGui::SetKeyboardFocusHere();
            *m_focus_requested = false;
        }

        // imgui provides utf-8 editing, selection, clipboard, and ime handling.
        ImGui::PushID(this);
        ImGui::SetNextItemWidth(-FLT_MIN);
        m_changed = ImGui::InputText("##value", m_value);
        ImGui::PopID();
        return true;
    }

    std::string* m_value;
    bool* m_focus_requested;
    bool m_changed = false;
};

TextInputWidget::TextInputWidget(std::string& value, std::string id)
    : Container(std::move(id), StackDirection::Horizontal), m_value(&value) {
    set_input_mode(InputMode::Target);
    set_size({grow(), fit()});

    set_type_name("TextInput");
    set_content_alignment(Anchor::CenterLeft);
    m_label_node = &add<TextWidget>("");
    m_label_node->set_id("label");
    m_label_node->set_visible(false);

    m_input_node = &add<Container>("input", StackDirection::Horizontal, "TextInputField");
    m_input_node->set_size({grow(), fit()});
    m_input_node->set_content_alignment(Anchor::CenterLeft);

    m_icon_node = &m_input_node->add<ImageWidget>();
    m_icon_node->set_id("icon");
    m_icon_node->set_enabled(false);
    m_icon_node->set_visible(false);

    m_field_node = &m_input_node->add<FieldNode>(value, m_focus_requested);
}

void TextInputWidget::on_event(UiEvent& event) {
    if (event.type == EventType::PointerDown && event.button == PointerButton::Left) {
        m_focus_requested = surface().input_router().set_focus(*this);
    }

    if (event.type == EventType::Cancel || (event.type == EventType::KeyDown && event.key == Key::Escape)) {
        const bool focused = input_state().focused;
        if (focused) {
            surface().input_router().restore_focus(*this);
            event.stop_propagation();
        } else {
            event.mark_handled();
        }
    }
}

void TextInputWidget::apply_theme_defaults(const Theme& theme) {
    Container::apply_theme_defaults(theme);
    set_font(surface().get_primary_font(18));
    const TransitionSpec transition{0.25F, easing::out_quad};
    m_label_spacing = {theme.metrics.item_inner_spacing.x, theme.metrics.item_spacing.y};
    update_label_layout();
    m_input_node->set_spacing(theme.metrics.item_inner_spacing.x);
    m_icon_node->set_size({px(18.0F), px(18.0F)});
    m_field_node->set_size({grow(), px(18.0F)});

    configure_all_styles([&theme, transition](Style& style) {
        style.border_color(theme.controls.border_color, transition)
            .padding({12.0F, 14.0F})
            .background_color(theme.controls.background_color, transition)
            .border(BORDER_ALL)
            .border_radius(4.0F)
            .border_thickness(theme.controls.border_thickness);
    });

    const auto configure_active_style = [&theme, transition](Style& style) {
        style.background_color(theme.controls.active_color, transition).border_color(theme.accent_color, transition);
    };
    configure_style(StyleType::ACTIVE, configure_active_style);
    configure_style(StyleType::FOCUS, configure_active_style);
    configure_style(StyleType::HOVER, [&theme, transition](Style& style) {
        style.background_color(theme.controls.hover_color, transition);
    });

    m_field_node->configure_all_styles([&theme](Style& style) {
        style.color(theme.text_color).background_color(theme.transparent).padding({}).border(BORDER_NONE);
    });
}

TextInputWidget& TextInputWidget::set_label(std::string label) {
    m_label_node->set_text(std::move(label));
    m_label_node->set_visible(!m_label_node->empty());
    update_label_layout();
    invalidate_measure();
    return *this;
}

TextInputWidget& TextInputWidget::set_label_placement(LabelPlacement placement) {
    if (m_label_placement == placement) {
        return *this;
    }

    m_label_placement = placement;
    update_label_layout();
    return *this;
}

TextInputWidget& TextInputWidget::set_icon(Texture* icon) {
    m_icon_node->set_texture(icon);
    m_icon_node->set_visible(icon != nullptr);
    return *this;
}

bool TextInputWidget::set_value(std::string value) {
    if (*m_value == value) {
        return false;
    }

    *m_value = std::move(value);
    notify_change();
    return true;
}

void TextInputWidget::update_label_layout() {
    const bool above = m_label_placement == LabelPlacement::Above;
    set_direction(above ? StackDirection::Vertical : StackDirection::Horizontal);
    set_spacing(above ? m_label_spacing.y : m_label_spacing.x);
    m_label_node->set_size({fit(), fit()});
    m_input_node->set_size({grow(), fit()});
}

void TextInputWidget::on_draw_end() {
    if (m_icon_node->visible()) {
        const ImVec4 icon_color =
            input_state().hovered || input_state().active ? surface().theme().text_color : surface().theme().text_secondary_color;
        m_icon_node->style().color(ImColor(icon_color));
    }

    if (m_field_node->changed()) {
        notify_change();
    }

    Container::on_draw_end();
}
