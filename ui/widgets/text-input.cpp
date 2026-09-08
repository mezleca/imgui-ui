#include "text-input.hpp"

#include "../style/theme.hpp"
#include "../ui.hpp"
#include "image.hpp"

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

TextInputWidget::TextInputWidget(UI& ui, std::string& value, std::string label)
    : StackContainer(std::move(label), StackDirection::Horizontal), m_ui(ui), m_value(&value) {
    set_input_mode(InputMode::Target);

    set_type_name("TextInput");
    set_content_alignment(Anchor::CenterLeft);
    set_font(ui.get_primary_font(18));

    m_icon_node = &add<ImageWidget>();
    m_icon_node->set_id("icon");
    m_icon_node->set_enabled(false);
    m_icon_node->set_visible(false);

    m_field_node = &add<FieldNode>(value, m_focus_requested);
    apply_theme_defaults(ui.theme());

    _on_event = [this](UiEvent& event) {
        if (event.type == EventType::PointerDown && event.button == PointerButton::Left) {
            m_focus_requested = m_ui.input_router().set_focus(*this);
        }

        if (event.type == EventType::Cancel || (event.type == EventType::KeyDown && event.key == Key::Escape)) {
            m_ui.input_router().clear_focus(*this);
            event.stop_propagation();
        }
    };
}

void TextInputWidget::apply_theme_defaults(const Theme& theme) {
    const ImVec2 icon_size = theme.widgets.text_input_icon_size;
    set_spacing(theme.widgets.text_input_icon_spacing);
    m_icon_node->set_size({px(icon_size.x), px(icon_size.y)});
    m_field_node->set_size({grow(), px(icon_size.y)});

    configure_all_styles([&theme](Style& style) {
        style.border_color(theme.border_color, 0.15F)
            .padding(theme.widgets.text_input_padding)
            .background_color(theme.background_secondary_color)
            .border(BORDER_ALL)
            .border_radius(theme.box_rounding)
            .border_thickness(theme.controls.border_thickness);
    });

    configure_style(StyleType::ACTIVE, [&theme](Style& style) { style.border_color(theme.accent_color); });
    configure_style(StyleType::FOCUS, [&theme](Style& style) { style.border_color(theme.accent_color); });
    configure_style(StyleType::HOVER, [&theme](Style& style) { style.border_color(theme.accent_color); });

    m_field_node->configure_all_styles([&theme](Style& style) {
        style.color(theme.text_color).background_color(theme.transparent).padding({}).border(BORDER_NONE);
    });
}

TextInputWidget& TextInputWidget::set_icon(Texture* icon) {
    m_icon_node->set_texture(icon);
    m_icon_node->set_visible(icon != nullptr);
    return *this;
}

bool TextInputWidget::set_value(std::string value) {
    if (m_value == nullptr || *m_value == value) {
        return false;
    }

    *m_value = std::move(value);
    notify_change();
    return true;
}

void TextInputWidget::on_measure() {
    ImVec2 size = layout().intrinsic_size();
    if (layout().size_spec().height.mode != LayoutSizeMode::Fixed && font() != nullptr && ImGui::GetCurrentContext() != nullptr) {
        ImGui::PushFont(font());
        size.y = ImGui::GetTextLineHeight() + style().padding().y * 2.0F;
        ImGui::PopFont();
    }

    set_measured_size(size, false, true);
}

void TextInputWidget::on_draw_end() {
    if (m_icon_node->visible()) {
        const ImVec4 icon_color =
            input_state().hovered || input_state().active ? m_ui.theme().text_color : m_ui.theme().text_secondary_color;
        m_icon_node->style().color().set(ImColor(icon_color));
    }

    if (m_field_node->changed()) {
        notify_change();
    }

    Container::on_draw_end();
}
