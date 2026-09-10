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
    DropdownBodyNode(DropdownWidget::State& state, UI& ui, InputRouter& input_router);

    void place_below(const Node& trigger) {
        const Rect rect = trigger.layout().visual_rect();
        m_popup_position = {rect.min.x, rect.max.y + m_state.popup_gap};
        m_popup_width = rect.size().x;
    }

    void rebuild_options();

private:
    friend class DropdownWidget;

    bool paint() override;
    void draw_children() override;
    void on_draw_end() override;
    void apply_theme_defaults(const Theme& theme) override;
    void on_update(float dt) override;

    DropdownWidget::State& m_state;
    UI& m_ui;
    ImVec2 m_popup_position{};
    float m_popup_width = 0.0F;
    float m_item_height = 0.0F;
    bool m_popup_opened = false;
    InputRouter& m_input_router;
};

class ui::DropdownOptionNode final : public ButtonWidget {
public:
    DropdownOptionNode(UI& ui, DropdownWidget::State& state, std::size_t index, const Theme& theme)
        : ButtonWidget(ui, state.options[index].label, {grow(), px(0.0F)}), m_state(state), m_index(index) {
        set_type_name("DropdownOption");
        set_text_alignment({0.0F, 0.5F});

        // this index is valid because set_options rebuilds rows when the option count changes.
        set_on_click([this] {
            if (m_state.is_open() && m_state.owner != nullptr) {
                m_state.owner->select_value(m_state.options[m_index].value);
            }
        });
        apply_theme_defaults(theme);
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
                .padding(theme.widgets.dropdown_item_padding)
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
    explicit DropdownTriggerNode(DropdownWidget::State& state) : DrawListWidget("trigger", "Dropdown"), m_state(state) {}

    void set_open(bool open) {
        if (open) {
            set_visual_style(StyleType::ACTIVE);
            return;
        }

        set_interaction_style(input_state().hovered, input_state().active, input_state().focused);
    }

private:
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
        draw_frame(draw_list, rect, current_style);

        const DropdownOption* selected = m_state.selected_option();
        const std::string_view preview = selected == nullptr ? m_state.placeholder : selected->label;
        const ImVec2 text_size = ImGui::CalcTextSize(preview.data(), preview.data() + preview.size());

        draw_text(
            draw_list, {rect.min.x + current_style.padding().x, rect.min.y + (rect.size().y - text_size.y) * 0.5F},
            current_style.color().get_col(), preview
        );

        draw_triangle(
            draw_list, {rect.max.x - current_style.padding().x - m_state.arrow_size.x * 0.5F, rect.min.y + rect.size().y * 0.5F},
            m_state.arrow_size, current_style.color().get_col(),
            m_state.is_open() ? TriangleDirection::Up : TriangleDirection::Down
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

DropdownBodyNode::DropdownBodyNode(DropdownWidget::State& state, UI& ui, InputRouter& input_router)
    : Widget("body", "DropdownBody", InputMode::None), m_state(state), m_ui(ui), m_input_router(input_router) {
    set_layout({.size = {px(0.0F), px(0.0F)}, .in_flow = false});
    fade_out();
    apply_theme_defaults(ui.theme());
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
        add<DropdownOptionNode>(m_ui, m_state, index, m_ui.theme());
    }
}

bool DropdownBodyNode::paint() {
    if (!m_state.is_open() && !m_state.is_closing()) {
        return false;
    }

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
    const ImVec2 padding = style.padding();

    ImGui::SetNextWindowPos(m_popup_position, ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        {m_popup_width + padding.x * 2.0F, m_item_height * static_cast<float>(children().size()) + padding.y * 2.0F}
    );
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{});
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, style.border_radius());
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0F);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4{});

    if (ImGui::BeginPopup("body", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
        const Rect body_rect = Rect::from_position_size(ImGui::GetWindowPos(), ImGui::GetWindowSize());
        set_visual_rect(body_rect);
        draw_frame(*ImGui::GetWindowDrawList(), body_rect, style);

        // block the body while keeping its option rows targetable.
        m_input_router.register_blocker(*this, body_rect);
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
    // use the resolved popup width for every option row.
    const ImVec2 padding = computed_style().padding();
    const float item_width = std::max(0.0F, layout().visual_rect().size().x - padding.x * 2.0F);

    for (std::size_t index = 0; index < children().size(); ++index) {
        auto& option_node = static_cast<DropdownOptionNode&>(*children()[index]);
        option_node.set_size({px(item_width), px(m_item_height)});
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

DropdownWidget::DropdownWidget(UI& ui, std::string& value, std::vector<DropdownOption> options, std::string id)
    : Widget(std::move(id), "Dropdown"), m_state{.value = &value, .options = std::move(options)} {
    m_state.owner = this;

    m_label_node = &add<TextWidget>("");
    m_trigger = &add<DropdownTriggerNode>(m_state);
    m_body = &add<DropdownBodyNode>(m_state, ui, ui.input_router());
    m_state.body = m_body;
    m_state.trigger = m_trigger;
    m_body->set_enabled(false);

    apply_theme_defaults(ui.theme());
}

void DropdownWidget::on_event(UiEvent& event) {
    // prevent imgui from handling the same press or release.
    if (event.type == EventType::PointerDown || event.type == EventType::PointerUp) {
        event.block_native_input();
    }
}

void DropdownWidget::apply_theme_defaults(const Theme& theme) {
    m_state.arrow_size = theme.widgets.dropdown_arrow_size;
    m_state.popup_gap = std::max(0.0F, theme.widgets.dropdown_popup_gap);
    m_state.transition_duration = std::max(0.0F, theme.widgets.dropdown_transition_duration);

    configure_all_styles([&theme](Style& style) {
        style.color(theme.text_color)
            .background_color(theme.transparent)
            .border_color(theme.controls.border_color)
            .border(BORDER_NONE)
            .border_radius(theme.controls.rounding)
            .border_thickness(theme.controls.border_thickness)
            .padding({});
    });

    m_label_node->style().color(theme.text_color);
    m_trigger->configure_all_styles([&theme](Style& style) { style.control(theme).cursor(ImGuiMouseCursor_Hand); });
    m_trigger->configure_style(StyleType::HOVER, [&theme](Style& style) { style.background_color(theme.controls.hover_color); });
    m_trigger->configure_style(StyleType::ACTIVE, [&theme](Style& style) {
        style.background_color(theme.controls.active_color);
    });
}

bool DropdownWidget::paint() {
    draw_frame(*ImGui::GetWindowDrawList(), layout().visual_rect(), computed_style());
    return true;
}

DropdownWidget& DropdownWidget::set_label(std::string label) {
    m_label_node->set_text(std::move(label));
    return *this;
}

bool DropdownWidget::select_value(std::string_view value) {
    const DropdownOption* option = m_state.find_option(value);
    if (option == nullptr || *m_state.value == option->value) {
        return false;
    }

    const std::size_t index = static_cast<std::size_t>(option - m_state.options.data());
    const bool changed = m_state.select(index);
    if (changed) {
        notify_change();
    }
    return changed;
}

DropdownWidget& DropdownWidget::set_placeholder(std::string placeholder) {
    if (m_state.placeholder == placeholder) {
        return *this;
    }

    m_state.placeholder = std::move(placeholder);
    return *this;
}

DropdownWidget& DropdownWidget::set_options(std::vector<DropdownOption> options) {
    if (m_state.options == options) {
        return *this;
    }

    m_state.options = std::move(options);
    m_body->rebuild_options();
    return *this;
}

void DropdownWidget::on_measure() {
    ImVec2 size = layout().intrinsic_size();
    const ImVec2 padding = computed_style().padding();
    if (layout().size_spec().height.mode != LayoutSizeMode::Fixed) {
        size.y = ImGui::GetTextLineHeight() + m_trigger->computed_style().padding().y * 2.0F + padding.y * 2.0F;
        if (has_label()) {
            size.y += m_label_node->layout().size().y + ImGui::GetStyle().ItemSpacing.y;
        }
    }

    set_measured_size(size, false, true);
}

Widget& DropdownWidget::trigger() {
    return *m_trigger;
}

Widget& DropdownWidget::body() {
    return *m_body;
}

void DropdownWidget::on_layout() {
    const float label_height = has_label() ? m_label_node->layout().size().y + ImGui::GetStyle().ItemSpacing.y : 0.0F;
    const ImVec2 outer_size = layout().size();
    const ImVec2 padding = computed_style().padding();
    const ImVec2 trigger_size = {
        std::max(0.0F, outer_size.x - padding.x * 2.0F),
        std::max(0.0F, outer_size.y - label_height - padding.y * 2.0F),
    };

    m_trigger->set_size({px(trigger_size.x), px(trigger_size.y)});
}

void DropdownWidget::draw_children() {
    const ImVec2 padding = computed_style().padding();
    const ImVec2 cursor = ImGui::GetCursorPos();
    ImGui::SetCursorPos({cursor.x + padding.x, cursor.y + padding.y});

    if (has_label()) {
        const float content_x = ImGui::GetCursorPosX();
        m_label_node->draw();
        ImGui::SetCursorPosX(content_x);
    }

    m_trigger->draw();
    m_body->place_below(*m_trigger);
    m_body->draw();
}

bool DropdownWidget::has_label() const {
    return !m_label_node->empty();
}
