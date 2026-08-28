#include "text-input.hpp"

#include "../style/theme.hpp"
#include "../ui.hpp"
#include "image.hpp"

#include <cfloat>
#include <imgui_stdlib.h>

static constexpr ImVec2 INPUT_ICON_SIZE = {18.0F, 18.0F};
static constexpr float INPUT_ICON_SPACING = 10.0F;

using namespace ui;

class TextInputWidget::FieldNode final : public StyledNode {
public:
    FieldNode(std::string& value, bool& focus_requested)
        : StyledNode("text", "TextInput"), m_value(&value), m_focus_requested(&focus_requested) {}

    bool accepts_input() const override {
        return false;
    }

    bool changed() const override {
        return m_changed;
    }

    std::optional<std::string> content() const override {
        return *m_value;
    }

    bool try_set_content(std::string content) override {
        if (*m_value == content) {
            return false;
        }

        *m_value = std::move(content);
        return true;
    }

private:
    bool on_draw() override {
        if (*m_focus_requested) {
            ImGui::SetKeyboardFocusHere();
            *m_focus_requested = false;
        }

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

TextInputWidget::TextInputWidget(UI& ui, std::string& value) : TextInputWidget(ui, value, {}) {}

TextInputWidget::TextInputWidget(UI& ui, std::string& value, std::string label)
    : StackContainer(std::move(label), StackDirection::Horizontal), m_ui(ui), m_value(&value) {
    const Theme& theme = m_ui.theme();

    set_type_name("TextInput");
    set_spacing(INPUT_ICON_SPACING);
    set_accepts_focus(true);
    set_center_content_vertically(true);
    set_font(ui.get_primary_font(18));
    configure_all_styles([&theme](Style& style) {
        style.border_color(theme.border_color, 0.15F)
            .padding({12.0F, 14.0F})
            .background_color(theme.background_secondary_color)
            .border(BORDER_ALL)
            .border_radius(theme.box_rounding);
    });

    m_icon_node = &add_child<ImageWidget>();
    m_icon_node->set_id("icon");
    m_icon_node->set_size(INPUT_ICON_SIZE);
    m_icon_node->set_enabled(false);
    m_icon_node->set_visible(false);

    configure_style(StyleType::ACTIVE, [&theme](Style& style) { style.border_color(theme.accent_color); });
    configure_style(StyleType::FOCUS, [&theme](Style& style) { style.border_color(theme.accent_color); });
    configure_style(StyleType::HOVER, [&theme](Style& style) { style.border_color(theme.accent_color); });

    m_field_node = &add_child<FieldNode>(value, m_focus_requested);
    m_field_node->set_size({0.0F, INPUT_ICON_SIZE.y});
    m_field_node->configure_all_styles([&theme](Style& style) {
        style.color(theme.text_color).background_color(theme.transparent).padding({}).border(BORDER_NONE);
    });

    _on_event = [this](UiEvent& event) {
        if (event.type == EventType::PointerDown && event.button == PointerButton::Left) {
            m_focus_requested = m_ui.input_router().set_focus(*this);
        }

        if (event.type != EventType::Cancel && !(event.type == EventType::KeyDown && event.key == Key::Escape)) {
            return;
        }

        m_ui.input_router().clear_focus(*this);
        event.stop_propagation();
    };
}

TextInputWidget& TextInputWidget::set_icon(IconTexture* icon) {
    m_icon_node->set_texture(icon);
    m_icon_node->set_visible(icon != nullptr);
    return *this;
}

void TextInputWidget::on_measure() {
    ImVec2 size = requested_size();
    if (size.y <= 0.0F && font() != nullptr && ImGui::GetCurrentContext() != nullptr) {
        ImGui::PushFont(font());
        size.y = ImGui::GetTextLineHeight() + style().padding().y * 2.0F;
        ImGui::PopFont();
    }

    set_size(size);
}

void TextInputWidget::on_layout() {
    if (!size_was_resolved()) {
        ImVec2 size = requested_size();
        const ImVec2 available = ImGui::GetContentRegionAvail();

        size.y = size.y > 0.0F ? size.y : ImGui::GetFontSize() + style().padding().y * 2.0F;
        resolve_size(resolve_layout_size(size, available));
    }

    StackContainer::on_layout();
}

void TextInputWidget::on_draw_end() {
    // the field supplies item geometry
    // the outer container owns interaction and focus.
    m_input_state = m_ui.input().observe_item(*this);
    m_input_state.hovered = m_input_state.hovered || ImGui::IsWindowHovered();

    if (m_icon_node->visible()) {
        const ImVec4 icon_color =
            m_input_state.hovered || m_input_state.active ? m_ui.theme().text_color : m_ui.theme().text_secondary_color;
        m_icon_node->style().color().set(ImColor(icon_color));
    }

    apply_input_state(m_input_state);
    if (m_field_node->changed()) {
        notify_change();
    }

    ChildContainer::on_draw_end();
}

const ItemInputState& TextInputWidget::input_state() const {
    return m_input_state;
}

bool TextInputWidget::changed() const {
    return m_field_node->changed();
}

std::optional<std::string> TextInputWidget::content() const {
    return m_value == nullptr ? std::nullopt : std::optional<std::string>{*m_value};
}

bool TextInputWidget::try_set_content(std::string content) {
    if (m_value == nullptr || *m_value == content) {
        return false;
    }

    *m_value = std::move(content);
    notify_change();
    return true;
}
