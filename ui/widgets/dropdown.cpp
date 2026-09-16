#include "dropdown.hpp"

#include "../imgui/draw.hpp"
#include "../style/theme.hpp"
#include "../ui.hpp"
#include "button.hpp"
#include "text.hpp"

#include <algorithm>
#include <imgui.h>
#include <utility>

using namespace ui;

class ui::DropdownBodyNode final : public Widget {
public:
    explicit DropdownBodyNode(DropdownWidget::State& state);

    void rebuild_options();

private:
    friend class DropdownWidget;

    bool paint() override;
    void draw_children() override;
    void on_draw_end() override;
    void apply_theme_defaults(const Theme& theme) override;
    void on_update(float dt) override;

    DropdownWidget::State& m_state;
    float m_item_height = 0.0F;
    bool m_popup_opened = false;
};

class ui::DropdownOptionNode final : public ButtonWidget {
public:
    DropdownOptionNode(DropdownWidget::State& state, std::size_t index)
        : ButtonWidget(state.options[index].label, {grow(), fit()}), m_state(state), m_index(index) {
        set_type_name("DropdownOption");
        set_text_alignment({0.0F, 0.5F});

        // this index is valid because set_options rebuilds rows when the option count changes.
        set_on_click([this] {
            if (m_state.is_open() && m_state.owner != nullptr) {
                m_state.owner->select_value(m_state.options[m_index].value);
            }
        });
    }

    bool accepts_input() const override {
        return m_state.is_open() && ButtonWidget::accepts_input();
    }

protected:
    void apply_theme_defaults(const Theme& theme) override {
        const float rounding = m_index == 0 || m_index + 1 == m_state.options.size() ? theme.controls.rounding : 0.0F;

        configure_all_styles([&theme, rounding](Style& style) {
            style.color(theme.text_color)
                .background_color(theme.transparent)
                .padding({10.0F, 4.0F})
                .border(BORDER_NONE)
                .border_radius(rounding)
                .cursor(ImGuiMouseCursor_Hand);
        });

        configure_style(StyleType::HOVER, [&theme](Style& style) { style.background_color(theme.controls.hover_color); });
        configure_style(StyleType::ACTIVE, [&theme](Style& style) { style.background_color(theme.controls.active_color); });
    }

private:
    DropdownWidget::State& m_state;
    std::size_t m_index;
};

class ui::DropdownTriggerNode final : public DrawListWidget {
public:
    explicit DropdownTriggerNode(DropdownWidget::State& state) : DrawListWidget("trigger", "DropdownTrigger"), m_state(state) {
        set_size({grow(), fit()});
    }

    void set_open(bool open) {
        if (open) {
            set_visual_style(StyleType::ACTIVE);
            return;
        }

        set_interaction_style(input_state().hovered, input_state().active, input_state().focused);
    }

private:
    void on_measure() override {
        const DropdownOption* selected = m_state.selected_option();
        const std::string_view preview = selected == nullptr ? m_state.placeholder : selected->label;
        const ImVec2 text_size = ImGui::CalcTextSize(preview.data(), preview.data() + preview.size());
        set_measured_content_size(
            {text_size.x + m_state.arrow_size.x + ImGui::GetStyle().ItemInnerSpacing.x,
             std::max(text_size.y, m_state.arrow_size.y)},
            true, true
        );
    }

    void on_click(UiEvent& event) override {
        if (event.button != PointerButton::Left) {
            return;
        }

        if (m_state.is_closed()) {
            m_state.open();
        } else {
            m_state.close();
        }
    }

    void paint_draw_list(ImDrawList& draw_list, Rect rect, const ComputedStyle& current_style) override {
        const DropdownOption* selected = m_state.selected_option();
        const std::string_view preview = selected == nullptr ? m_state.placeholder : selected->label;
        const ImVec2 text_size = ImGui::CalcTextSize(preview.data(), preview.data() + preview.size());
        const Rect content = content_rect(rect);

        draw_text(
            draw_list, {content.min.x, content.min.y + (content.size().y - text_size.y) * 0.5F}, current_style.color().get_col(),
            preview
        );

        draw_triangle(
            draw_list, {content.max.x - m_state.arrow_size.x * 0.5F, content.min.y + content.size().y * 0.5F}, m_state.arrow_size,
            current_style.color().get_col(), m_state.is_open() ? TriangleDirection::Up : TriangleDirection::Down
        );
    }

    void input_state_changed() override {
        StyledNode::input_state_changed();
        if (m_state.is_open()) {
            set_visual_style(StyleType::ACTIVE);
        }
    }

    DropdownWidget::State& m_state;
};

DropdownBodyNode::DropdownBodyNode(DropdownWidget::State& state)
    : Widget("body", "DropdownBody", InputMode::None), m_state(state) {
    set_layout({.size = {px(0.0F), px(0.0F)}, .in_flow = false});
    fade_out();
    rebuild_options();
}

void DropdownBodyNode::apply_theme_defaults(const Theme& theme) {
    configure_all_styles([&theme](Style& style) {
        style.color(theme.text_color)
            .background_color(theme.controls.background_color)
            .border(BORDER_ALL)
            .border_color(theme.controls.border_color)
            .border_radius(theme.controls.rounding)
            .border_thickness(theme.controls.border_thickness)
            .padding({});
    });
}

void DropdownBodyNode::rebuild_options() {
    if (children().size() == m_state.options.size()) {
        for (std::size_t index = 0; index < children().size(); ++index) {
            static_cast<DropdownOptionNode&>(*children()[index]).set_text(m_state.options[index].label);
        }
        return;
    }

    clear();
    for (std::size_t index = 0; index < m_state.options.size(); ++index) {
        add<DropdownOptionNode>(m_state, index);
    }
}

bool DropdownBodyNode::paint() {
    if (!m_state.is_open() && !m_state.is_closing()) {
        return false;
    }

    const Rect trigger_rect = m_state.trigger->layout().visual_rect();
    const ImVec2 popup_position = {trigger_rect.min.x, trigger_rect.max.y + m_state.popup_gap};
    const float popup_width = trigger_rect.size().x;

    ImGui::PushID(this);

    // open the imgui popup once per open cycle.
    if (m_state.is_open() && !m_popup_opened) {
        ImGui::OpenPopup("body");
        m_popup_opened = true;
    }

    const ComputedStyle& style = computed_style();
    const ImVec2 item_padding =
        children().empty() ? ImVec2{} : static_cast<const DropdownOptionNode&>(*children().front()).computed_style().padding();
    m_item_height = ImGui::GetTextLineHeight() + item_padding.y * 2.0F;
    ImGui::SetNextWindowPos(popup_position, ImGuiCond_Always);
    ImGui::SetNextWindowSize(outer_size({popup_width, m_item_height * static_cast<float>(children().size())}));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.padding());
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{});
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, style.border_radius());
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0F);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4{});

    if (ImGui::BeginPopup("body", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
        const Rect body_rect = Rect::from_position_size(ImGui::GetWindowPos(), ImGui::GetWindowSize());
        set_visual_rect(body_rect);
        draw_surface(*ImGui::GetWindowDrawList(), body_rect);

        // block the body while keeping its option rows targetable.
        surface().input_router().register_blocker(*this, body_rect);
        return true;
    } else if (m_state.is_open()) {
        m_state.close();
        m_popup_opened = false;
    }

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);
    ImGui::PopID();
    return false;
}

void DropdownBodyNode::draw_children() {
    const float item_width = content_size(layout().visual_rect().size()).x;
    const ImVec2 item_size = {item_width, m_item_height};

    for (std::size_t index = 0; index < children().size(); ++index) {
        auto& option_node = static_cast<DropdownOptionNode&>(*children()[index]);
        arrange_child(option_node, item_size, {.offset = {0.0F, m_item_height * static_cast<float>(index)}});
        const InputState& input = option_node.input_state();

        if (!input.hovered && !input.active && !input.focused) {
            const bool selected = m_state.options[index].value == *m_state.value;
            option_node.set_visual_style(selected ? StyleType::ACTIVE : StyleType::DEFAULT);
        }

        option_node.draw();
    }
}

void DropdownBodyNode::on_draw_end() {
    const ComputedStyle& style = computed_style();
    ImColor border = style.border_color().value;
    border.Value.w *= std::clamp(ImGui::GetStyle().Alpha, 0.0F, 1.0F);
    draw_border(*ImGui::GetWindowDrawList(), layout().visual_rect(), style, border);

    ImGui::EndPopup();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);
    ImGui::PopID();
}

void DropdownBodyNode::on_update(float) {
    if (!m_state.is_closing() || opacity() > VISIBILITY_OPACITY_THRESHOLD) {
        return;
    }

    m_state.finish_close();
    m_popup_opened = false;
}

const DropdownOption* DropdownWidget::State::find_option(std::string_view option_value) const {
    const auto option = std::find_if(options.begin(), options.end(), [option_value](const DropdownOption& candidate) {
        return candidate.value == option_value;
    });
    return option == options.end() ? nullptr : &*option;
}

const DropdownOption* DropdownWidget::State::selected_option() const {
    return value == nullptr ? nullptr : find_option(*value);
}

void DropdownWidget::open() {
    m_state.open();
}

void DropdownWidget::close() {
    m_state.close();
}

bool DropdownWidget::State::select(std::size_t index) {
    if (index >= options.size()) {
        return false;
    }

    const bool result = *value != options[index].value;

    if (result) {
        *value = options[index].value;
    }

    close();
    return result;
}

void DropdownWidget::State::open() {
    if (visibility != Visibility::Closed) {
        return;
    }

    visibility = Visibility::Open;

    trigger->set_open(true);
    body->set_enabled(true);
    body->fade_in({transition_duration, easing::linear});
}

void DropdownWidget::State::close() {
    if (visibility != Visibility::Open) {
        return;
    }

    visibility = Visibility::Closing;
    trigger->set_open(false);

    // keep the body registered while it fades out. closing rows cannot be selected.
    body->set_enabled(true);
    body->fade_out({transition_duration, easing::linear});
}

void DropdownWidget::State::finish_close() {
    visibility = Visibility::Closed;
    body->set_enabled(false);
}

DropdownWidget::DropdownWidget(std::string& value, std::vector<DropdownOption> options, std::string id)
    : Container(std::move(id), StackDirection::Vertical), m_state{.value = &value, .options = std::move(options)} {
    set_type_name("Dropdown");
    set_size({fit(), fit()});
    set_input_mode(InputMode::Target);
    m_state.owner = this;

    m_label_node = &add<TextWidget>("");
    m_label_node->set_visible(false);
    m_trigger = &add<DropdownTriggerNode>(m_state);
    m_body = &add<DropdownBodyNode>(m_state);
    m_state.body = m_body;
    m_state.trigger = m_trigger;
    m_body->set_enabled(false);
}

void DropdownWidget::on_event(UiEvent& event) {
    // prevent imgui from handling the same press or release.
    if (event.type == EventType::PointerDown || event.type == EventType::PointerUp) {
        event.block_native_input();
    }
}

void DropdownWidget::apply_theme_defaults(const Theme& theme) {
    Container::apply_theme_defaults(theme);
    m_state.arrow_size = {8.0F, 4.0F};
    m_state.popup_gap = 4.0F;
    m_state.transition_duration = 0.06F;

    configure_all_styles([&theme](Style& style) {
        style.color(theme.text_color)
            .background_color(theme.transparent)
            .border_color(theme.controls.border_color)
            .border(BORDER_NONE)
            .border_radius(theme.controls.rounding)
            .border_thickness(theme.controls.border_thickness)
            .padding({});
    });

    set_spacing(theme.metrics.item_spacing.y);
    m_label_node->style().color(theme.text_color);
    m_trigger->configure_all_styles(
        [&theme](Style& style) { style.control(theme, {10.0F, 6.0F}).cursor(ImGuiMouseCursor_Hand); }
    );
    m_trigger->configure_style(StyleType::HOVER, [&theme](Style& style) { style.background_color(theme.controls.hover_color); });
    m_trigger->configure_style(StyleType::ACTIVE, [&theme](Style& style) {
        style.background_color(theme.controls.active_color);
    });
}

DropdownWidget& DropdownWidget::set_label(std::string label) {
    m_label_node->set_visible(!label.empty());
    m_label_node->set_text(std::move(label));
    return *this;
}

bool DropdownWidget::select_value(std::string_view value) {
    const DropdownOption* option = m_state.find_option(value);
    if (option == nullptr || *m_state.value == option->value) {
        return false;
    }

    const std::size_t index = static_cast<std::size_t>(option - m_state.options.data());
    if (!m_state.select(index)) {
        return false;
    }

    invalidate_measure();
    notify_change();
    return true;
}

DropdownWidget& DropdownWidget::set_placeholder(std::string placeholder) {
    if (m_state.placeholder == placeholder) {
        return *this;
    }

    m_state.placeholder = std::move(placeholder);
    invalidate_measure();
    return *this;
}

DropdownWidget& DropdownWidget::set_options(std::vector<DropdownOption> options) {
    if (m_state.options == options) {
        return *this;
    }

    m_state.options = std::move(options);
    m_body->rebuild_options();
    invalidate_measure();
    return *this;
}

Widget& DropdownWidget::trigger() {
    return *m_trigger;
}

Widget& DropdownWidget::body() {
    return *m_body;
}
