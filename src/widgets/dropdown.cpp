#include <imgui-ui/widgets/dropdown.hpp>

#include <imgui-ui/imgui/draw.hpp>
#include <imgui-ui/style/theme.hpp>
#include <imgui-ui/surface.hpp>
#include <imgui-ui/widgets/button.hpp>
#include <imgui-ui/widgets/text.hpp>

#include <algorithm>
#include <imgui.h>
#include <utility>

using namespace ui;

class ui::DropdownWidget::Body final : public Widget {
public:
    explicit Body(DropdownWidget& owner);

    void rebuild_options();

private:
    bool paint() override;
    void draw_children() override;
    void on_draw_end() override;
    void apply_theme_defaults(const Theme& theme) override;
    void on_update(float dt) override;

    DropdownWidget& m_owner;
    float m_item_height = 0.0F;
    bool m_popup_opened = false;
};

class ui::DropdownWidget::Option final : public ButtonWidget {
public:
    Option(DropdownWidget& owner, std::size_t index)
        : ButtonWidget(owner.m_options[index].label, {grow(), fit()}), m_owner(owner), m_index(index) {
        set_type_name("DropdownOption");
        set_text_alignment({0.0F, 0.5F});

        // each live option keeps its source index. count changes replace rows through deferred node removal.
        on_click([this] {
            if (m_owner.is_open()) {
                m_owner.select_value(m_owner.m_options[m_index].value);
            }
        });
    }

    bool accepts_input() const override {
        return m_owner.is_open() && ButtonWidget::accepts_input();
    }

protected:
    void apply_theme_defaults(const Theme& theme) override {
        const float rounding = m_index == 0 || m_index + 1 == m_owner.m_options.size() ? theme.controls.rounding : 0.0F;

        configure_all_styles([&theme, rounding](Style& style) {
            style.color(theme.text_color)
                .background_color(theme.transparent)
                .padding({10.0F, 4.0F})
                .border(BORDER_NONE)
                .border_radius(rounding)
                .cursor(ImGuiMouseCursor_Hand);
        });

        style(StyleType::HOVER).background_color(theme.controls.hover_color);
        style(StyleType::ACTIVE).background_color(theme.controls.active_color);
    }

private:
    DropdownWidget& m_owner;
    std::size_t m_index;
};

class ui::DropdownWidget::Trigger final : public DrawListWidget {
public:
    explicit Trigger(DropdownWidget& owner) : DrawListWidget("trigger", "DropdownTrigger"), m_owner(owner) {
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
        const DropdownOption* selected = m_owner.selected_option();
        const std::string_view preview = selected == nullptr ? m_owner.m_placeholder : selected->label;
        const ImVec2 text_size = ImGui::CalcTextSize(preview.data(), preview.data() + preview.size());
        set_measured_content_size(
            {text_size.x + m_owner.m_arrow_size.x + ImGui::GetStyle().ItemInnerSpacing.x,
             std::max(text_size.y, m_owner.m_arrow_size.y)},
            true, true
        );
    }

    void click_event(UiEvent& event) override {
        if (event.button != PointerButton::Left) {
            return;
        }

        if (m_owner.m_visibility == Visibility::Closed) {
            m_owner.open();
        } else {
            m_owner.close();
        }
    }

    void paint_content(const PaintContext& context) override {
        const ComputedStyle& current_style = context.style;
        const DropdownOption* selected = m_owner.selected_option();
        const std::string_view preview = selected == nullptr ? m_owner.m_placeholder : selected->label;
        const ImVec2 text_size = ImGui::CalcTextSize(preview.data(), preview.data() + preview.size());
        const Rect content = context.content_rect;

        context.painter.text(
            {content.min.x, content.min.y + ((content.size().y - text_size.y) * 0.5F)}, current_style.color().value, preview
        );

        context.painter.triangle(
            {content.max.x - (m_owner.m_arrow_size.x * 0.5F), content.min.y + (content.size().y * 0.5F)}, m_owner.m_arrow_size,
            current_style.color().value, m_owner.is_open() ? TriangleDirection::Up : TriangleDirection::Down
        );
    }

    void input_state_changed() override {
        StyledNode::input_state_changed();
        if (m_owner.is_open()) {
            set_visual_style(StyleType::ACTIVE);
        }
    }

    DropdownWidget& m_owner;
};

DropdownWidget::Body::Body(DropdownWidget& owner) : Widget("body", "DropdownBody", InputMode::None), m_owner(owner) {
    set_layout({.size = {px(0.0F), px(0.0F)}, .in_flow = false});
    fade_out();
    rebuild_options();
}

void DropdownWidget::Body::apply_theme_defaults(const Theme& theme) {
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

void DropdownWidget::Body::rebuild_options() {
    const auto live_count =
        std::count_if(children().begin(), children().end(), [](const auto& child) { return !child->removal_pending(); });
    if (static_cast<std::size_t>(live_count) == m_owner.m_options.size()) {
        std::size_t index = 0;
        for (const auto& child : children()) {
            if (child->removal_pending()) continue;

            static_cast<Option&>(*child).set_text(m_owner.m_options[index++].label);
        }
        return;
    }

    clear();
    for (std::size_t index = 0; index < m_owner.m_options.size(); ++index) {
        add<Option>(m_owner, index);
    }
}

bool DropdownWidget::Body::paint() {
    if (m_owner.m_visibility == Visibility::Closed) {
        return false;
    }

    const Rect trigger_rect = m_owner.m_trigger->layout().visual_rect();
    const ImVec2 popup_position = {trigger_rect.min.x, trigger_rect.max.y + m_owner.m_popup_gap};
    const float popup_width = trigger_rect.size().x;

    ImGui::PushID(this);

    // open the native popup once for each closed-to-open cycle. keep it drawn while the framework body fades out.
    if (m_owner.is_open() && !m_popup_opened) {
        ImGui::OpenPopup("body");
        m_popup_opened = true;
    }

    const ComputedStyle& style = computed_style();
    const auto first_row =
        std::find_if(children().begin(), children().end(), [](const auto& child) { return !child->removal_pending(); });
    const ImVec2 item_padding =
        first_row == children().end() ? ImVec2{} : static_cast<const Option&>(**first_row).computed_style().padding();
    m_item_height = ImGui::GetTextLineHeight() + (item_padding.y * 2.0F);
    ImGui::SetNextWindowPos(popup_position, ImGuiCond_Always);
    ImGui::SetNextWindowSize(outer_size({popup_width, m_item_height * static_cast<float>(m_owner.m_options.size())}));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, box_insets().window_padding());
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{});
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, style.border_radius());
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0F);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4{});

    if (ImGui::BeginPopup("body", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
        const Rect body_rect = Rect::from_position_size(ImGui::GetWindowPos(), ImGui::GetWindowSize());
        set_visual_rect(body_rect);
        draw_surface(body_rect);

        // block the body while keeping its option rows targetable.
        surface().input_router().register_blocker(*this, body_rect);
        return true;
    }
    if (m_owner.is_open()) {
        m_owner.close();
        m_popup_opened = false;
    }

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);
    ImGui::PopID();
    return false;
}

void DropdownWidget::Body::draw_children() {
    const float item_width = content_size(layout().visual_rect().size()).x;
    const ImVec2 item_size = {item_width, m_item_height};

    std::size_t index = 0;
    for (const auto& child : children()) {
        if (child->removal_pending()) {
            continue;
        }

        auto& option_node = static_cast<Option&>(*child);
        arrange_child(option_node, item_size, {.offset = {0.0F, m_item_height * static_cast<float>(index)}});
        const InputState& input = option_node.input_state();

        if (!input.hovered && !input.active && !input.focused) {
            const bool selected = m_owner.m_options[index].value == m_owner.m_value;
            option_node.set_visual_style(selected ? StyleType::ACTIVE : StyleType::DEFAULT);
        }

        option_node.draw();
        ++index;
    }
}

void DropdownWidget::Body::on_draw_end() {
    const ComputedStyle& style = computed_style();
    painter().border(layout().visual_rect(), style, style.border_color().value);

    ImGui::EndPopup();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);
    ImGui::PopID();
}

void DropdownWidget::Body::on_update(float) {
    if (m_owner.m_visibility != Visibility::Closing || opacity() > VISIBILITY_OPACITY_THRESHOLD) {
        return;
    }

    m_owner.finish_close();
    m_popup_opened = false;
}

const DropdownOption* DropdownWidget::find_option(std::string_view option_value) const {
    const auto option = std::find_if(m_options.begin(), m_options.end(), [option_value](const DropdownOption& candidate) {
        return candidate.value == option_value;
    });
    return option == m_options.end() ? nullptr : &*option;
}

const DropdownOption* DropdownWidget::selected_option() const {
    return find_option(m_value);
}

void DropdownWidget::open() {
    if (m_visibility != Visibility::Closed) {
        return;
    }

    m_visibility = Visibility::Open;

    m_trigger->set_open(true);
    m_body->set_enabled(true);
    m_body->fade_in({m_transition_duration, easing::linear});
}

void DropdownWidget::close() {
    if (m_visibility != Visibility::Open) {
        return;
    }

    m_visibility = Visibility::Closing;
    m_trigger->set_open(false);

    // keep the body registered while it fades out. closing rows cannot be selected.
    m_body->set_enabled(true);
    m_body->fade_out({m_transition_duration, easing::linear});
}

void DropdownWidget::finish_close() {
    m_visibility = Visibility::Closed;
    m_body->set_enabled(false);
}

DropdownWidget::DropdownWidget(std::string& value, std::vector<DropdownOption> options, std::string id)
    : Container(std::move(id), "Dropdown"), m_value(value), m_options(std::move(options)) {
    set_size({fit(), fit()});

    m_label_node = &add<TextWidget>("");
    m_label_node->set_visible(false);
    m_trigger = &add<Trigger>(*this);
    m_body = &add<Body>(*this);
    m_body->set_enabled(false);
}

void DropdownWidget::event(UiEvent& event) {
    if (event.type == EventType::PointerDown || event.type == EventType::PointerUp) {
        event.block_native_input();
    }
}

void DropdownWidget::apply_theme_defaults(const Theme& theme) {
    Container::apply_theme_defaults(theme);
    m_arrow_size = {8.0F, 4.0F};
    m_popup_gap = 4.0F;
    m_transition_duration = 0.06F;

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
    m_trigger->configure_all_styles([&theme](Style& style) {
        style.control(theme, {10.0F, 6.0F}).cursor(ImGuiMouseCursor_Hand);
    });
    m_trigger->style(StyleType::HOVER).background_color(theme.controls.hover_color);
    m_trigger->style(StyleType::ACTIVE).background_color(theme.controls.active_color);
}

DropdownWidget& DropdownWidget::set_label(std::string label) {
    m_label_node->set_visible(!label.empty());
    m_label_node->set_text(std::move(label));
    return *this;
}

bool DropdownWidget::select_value(std::string_view value) {
    const DropdownOption* option = find_option(value);
    if (option == nullptr || m_value == option->value) {
        return false;
    }

    m_value = option->value;
    close();
    invalidate_measure();
    notify_change();
    return true;
}

DropdownWidget& DropdownWidget::set_placeholder(std::string placeholder) {
    if (m_placeholder == placeholder) {
        return *this;
    }

    m_placeholder = std::move(placeholder);
    invalidate_measure();
    return *this;
}

DropdownWidget& DropdownWidget::set_options(std::vector<DropdownOption> options) {
    if (m_options == options) {
        return *this;
    }

    m_options = std::move(options);
    m_body->rebuild_options();
    invalidate_measure();
    return *this;
}

Widget& DropdownWidget::trigger() const {
    return *m_trigger;
}

Widget& DropdownWidget::body() const {
    return *m_body;
}
