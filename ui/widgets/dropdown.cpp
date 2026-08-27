#include "dropdown.hpp"
#include "../style/theme.hpp"
#include "../ui.hpp"
#include "../imgui/draw.hpp"
#include "text.hpp"

#include <imgui.h>
#include <algorithm>

using namespace ui;

class ui::DropdownTriggerNode final : public Widget {
public:
    DropdownTriggerNode(
        UI& ui, std::string& value, std::vector<DropdownOption>& options, std::string& placeholder, bool& open, bool& closing,
        bool& open_requested, Rect& trigger_rect
    )
        : Widget("trigger", "Dropdown"), m_ui(ui), m_value(&value), m_options(&options), m_placeholder(&placeholder),
          m_open(&open), m_closing(&closing), m_open_requested(&open_requested), m_trigger_rect(&trigger_rect) {}

private:
    bool on_draw() override {
        const Style& current_style = style();
        ImVec2 button_size = layout().size();

        if (button_size.x <= 0.0F) button_size.x = ImGui::GetContentRegionAvail().x;
        if (button_size.y <= 0.0F) button_size.y = ImGui::GetFrameHeight();

        ImGui::InvisibleButton("##trigger", button_size);
        *m_trigger_rect = {ImGui::GetItemRectMin(), ImGui::GetItemRectMax()};

        if (ImGui::IsItemClicked()) {
            if (*m_open) {
                *m_open = false;
                *m_closing = true;
            } else {
                *m_open = true;
                *m_closing = false;
                *m_open_requested = true;
            }
        }

        const auto selected = std::find_if(m_options->begin(), m_options->end(), [this](const DropdownOption& option) {
            return option.value == *m_value;
        });

        const std::string_view preview = selected == m_options->end() ? *m_placeholder : selected->label;
        draw_contents(*m_trigger_rect, preview, *m_open, current_style);
        update_input(m_ui.input(), *m_open);

        return true;
    }

    void draw_contents(Rect rect, std::string_view preview, bool open, const Style& current_style) const {
        draw_frame(
            rect, current_style, open ? ImColor(m_ui.theme().control_active_color) : current_style.background_color().value
        );

        const ImVec2 text_size = ImGui::CalcTextSize(preview.data(), preview.data() + preview.size());

        draw_text(
            {rect.min.x + current_style.padding().x, rect.min.y + (rect.size().y - text_size.y) * 0.5F},
            current_style.color().get_col(), preview
        );

        draw_triangle(
            {rect.max.x - current_style.padding().x - 4.0F, rect.min.y + rect.size().y * 0.5F}, {8.0F, 4.0F},
            current_style.color().get_col(), open ? TriangleDirection::Up : TriangleDirection::Down
        );
    }

    UI& m_ui;
    std::string* m_value;
    std::vector<DropdownOption>* m_options;
    std::string* m_placeholder;
    bool* m_open;
    bool* m_closing;
    bool* m_open_requested;
    Rect* m_trigger_rect;
};

class ui::DropdownBodyNode final : public Widget {
public:
    DropdownBodyNode(
        UI& ui, std::string& value, std::vector<DropdownOption>& options, Rect& trigger_rect, bool& open, bool& closing,
        bool& open_requested
    )
        : Widget("body", "Dropdown"), m_ui(ui), m_value(&value), m_options(&options), m_trigger_rect(&trigger_rect),
          m_open(&open), m_closing(&closing), m_open_requested(&open_requested) {}

    void draw() override {
        if (*m_open_requested) fade_in();
        Widget::draw();
    }

    bool changed() const override {
        return m_changed;
    }

private:
    bool on_draw() override {
        m_changed = false;
        set_enabled(*m_open);

        if (*m_closing) {
            set_opacity(VISIBILITY_OPACITY_THRESHOLD);
        }

        const Theme& theme = m_ui.theme();

        if (*m_open_requested) {
            set_enabled(true);
            fade_in();
            ImGui::OpenPopup("##options");
            *m_open_requested = false;
        }

        if (!ImGui::IsPopupOpen("##options")) {
            fade_out();
            set_enabled(false);
            *m_open = false;
            *m_closing = false;
            return false;
        }

        const Style& current_style = style();
        const ImVec2 default_position = {m_trigger_rect->min.x, m_trigger_rect->max.y + 4.0F};
        const ImVec2 popup_position = layout().has_explicit_position() ? layout().screen_rect().min : default_position;
        const ImVec2 body_size = layout().size();

        const float item_height = ImGui::GetTextLineHeight() + current_style.padding().y * 2.0F;
        const float popup_width = body_size.x > 0.0F ? body_size.x : m_trigger_rect->size().x;
        const float popup_border = current_style.border() == BORDER_NONE ? 0.0F : current_style.border_thickness();

        ImGui::SetNextWindowPos(popup_position, ImGuiCond_Always);
        ImGui::SetNextWindowSize({popup_width, body_size.y}, ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, current_style.padding());
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, popup_border);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, current_style.border_radius());
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{});
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableRounding, current_style.border_radius());
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2{0.0F, 0.5F});
        ImGui::PushStyleColor(ImGuiCol_PopupBg, current_style.background_color().get_col());
        ImGui::PushStyleColor(ImGuiCol_Border, current_style.border_color().get_col());
        ImGui::PushStyleColor(ImGuiCol_Text, current_style.color().get_col());
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4{});
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4{});
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4{});

        const ImGuiWindowFlags popup_flags = *m_closing ? ImGuiWindowFlags_NoInputs : ImGuiWindowFlags_None;
        if (ImGui::BeginPopup("##options", popup_flags)) {
            if (*m_closing && opacity() <= VISIBILITY_OPACITY_THRESHOLD) {
                ImGui::CloseCurrentPopup();
                fade_out();
                *m_closing = false;
            } else {
                for (const DropdownOption& option : *m_options) {
                    const ImVec2 item_size = {ImGui::GetContentRegionAvail().x, item_height};
                    const Rect item_rect = Rect::from_position_size(ImGui::GetCursorScreenPos(), item_size);
                    const bool hovered = ImGui::IsMouseHoveringRect(item_rect.min, item_rect.max);
                    const bool is_selected = option.value == *m_value;
                    const ImU32 text_color = hovered       ? ImGui::GetColorU32(theme.accent_hover_color)
                                             : is_selected ? ImGui::GetColorU32(theme.accent_color)
                                                           : current_style.color().get_col();
                    ImGui::PushStyleColor(ImGuiCol_Text, text_color);

                    const bool pressed =
                        ImGui::Selectable(option.label.c_str(), is_selected, ImGuiSelectableFlags_NoAutoClosePopups, item_size);
                    ImGui::PopStyleColor();

                    if (pressed) {
                        if (!is_selected) {
                            *m_value = option.value;
                            m_changed = true;
                        }
                        set_enabled(false);
                        set_opacity(VISIBILITY_OPACITY_THRESHOLD);
                        *m_open = false;
                        *m_closing = true;
                        break;
                    }

                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
            }

            const Rect body_rect = Rect::from_position_size(ImGui::GetWindowPos(), ImGui::GetWindowSize());
            set_screen_rect(body_rect);
            m_ui.input_router().register_region_in_layer(*this, body_rect, InputLayer::Overlay);

            ImGui::EndPopup();
        }

        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(6);
        return true;
    }

    UI& m_ui;
    std::string* m_value;
    std::vector<DropdownOption>* m_options;
    Rect* m_trigger_rect;
    bool* m_open;
    bool* m_closing;
    bool* m_open_requested;
    bool m_changed = false;
};

DropdownWidget::DropdownWidget(UI& ui, std::string& value, std::vector<DropdownOption> options, std::string id)
    : Widget(std::move(id), "Dropdown"), m_ui(ui), m_value(&value), m_options(std::move(options)) {
    m_label_node = &add_child<TextWidget>("");
    m_trigger = &add_child<DropdownTriggerNode>(
        m_ui, value, m_options, m_placeholder, m_open, m_closing, m_open_requested, m_trigger_rect
    );
    m_body = &add_child<DropdownBodyNode>(m_ui, value, m_options, m_trigger_rect, m_open, m_closing, m_open_requested);
    m_body->set_enabled(false);
    configure_default_styles();
}

void DropdownWidget::configure_default_styles() {
    const Theme& theme = m_ui.theme();

    const auto configure = [&theme](Widget& widget) {
        widget.configure_all_styles([&theme](Style& style) {
            style.color(theme.text_color)
                .background_color(theme.control_background_color)
                .border_color(theme.control_border_color, 0.15F)
                .padding({10.0F, 6.0F})
                .border(BORDER_ALL)
                .border_radius(theme.control_rounding)
                .border_thickness(theme.control_border_thickness);
        });
    };

    m_label_node->style().color(theme.text_color);

    configure(*m_trigger);
    configure(*m_body);

    m_trigger->configure_style(StyleType::HOVER, [&theme](Style& style) { style.background_color(theme.control_hover_color); });
    m_trigger->configure_style(StyleType::ACTIVE, [&theme](Style& style) { style.background_color(theme.control_active_color); });
}

DropdownWidget& DropdownWidget::set_label(std::string label) {
    m_label_node->try_set_content(std::move(label));
    return *this;
}

DropdownWidget& DropdownWidget::set_label_placement(DropdownLabelPlacement placement) {
    if (m_label_placement == placement) {
        return *this;
    }

    m_label_placement = placement;
    invalidate_measure();
    return *this;
}

DropdownWidget& DropdownWidget::set_placeholder(std::string placeholder) {
    m_placeholder = std::move(placeholder);
    return *this;
}

DropdownWidget& DropdownWidget::set_options(std::vector<DropdownOption> options) {
    m_options = std::move(options);
    return *this;
}

void DropdownWidget::on_measure() {
    ImVec2 size = requested_size();
    if (size.y <= 0.0F) {
        size.y = ImGui::GetFrameHeight();
        if (has_label()) {
            size.y += m_label_node->layout().size().y + ImGui::GetStyle().ItemSpacing.y;
        }
    }

    set_size(size);
}

bool DropdownWidget::changed() const {
    return m_body->changed();
}

Widget& DropdownWidget::trigger() {
    return *m_trigger;
}

Widget& DropdownWidget::body() {
    return *m_body;
}

bool DropdownWidget::on_draw() {
    ImGui::PushID(this);
    return true;
}

void DropdownWidget::on_draw_end() {
    ImGui::PopID();
}

void DropdownWidget::on_layout() {
    const bool inline_label = has_label() && m_label_placement == DropdownLabelPlacement::Inline;
    const float label_width = inline_label ? m_label_node->layout().size().x + ImGui::GetStyle().ItemInnerSpacing.x : 0.0F;
    const float label_height = has_label() && m_label_placement == DropdownLabelPlacement::Above
                                   ? m_label_node->layout().size().y + ImGui::GetStyle().ItemSpacing.y
                                   : 0.0F;
    const ImVec2 outer_size = layout().size();
    const ImVec2 trigger_size = {
        std::max(0.0F, outer_size.x - label_width),
        std::max(0.0F, outer_size.y - label_height),
    };

    m_trigger->set_size(trigger_size);
}

void DropdownWidget::draw_children() {
    const float content_x = ImGui::GetCursorPosX();

    if (has_label()) {
        m_label_node->draw();

        if (m_label_placement == DropdownLabelPlacement::Inline) {
            ImGui::SameLine();
        } else {
            ImGui::SetCursorPosX(content_x);
        }
    }

    // body remains a child even though imgui renders it in a separate popup window.
    m_trigger->draw();
    m_body->draw();
}

bool DropdownWidget::has_label() const {
    const std::optional<std::string> text = m_label_node->content();
    return text.has_value() && !text->empty();
}

std::optional<std::string> DropdownWidget::content() const {
    return m_value == nullptr ? std::nullopt : std::optional<std::string>{*m_value};
}

bool DropdownWidget::try_set_content(std::string content) {
    if (m_value == nullptr || *m_value == content ||
        std::none_of(m_options.begin(), m_options.end(), [&content](const DropdownOption& option) {
            return option.value == content;
        })) {
        return false;
    }

    *m_value = std::move(content);
    return true;
}
