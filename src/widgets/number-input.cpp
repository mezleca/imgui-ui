#include <imgui-ui/widgets/number-input.hpp>
#include <imgui-ui/imgui/painter.hpp>
#include <imgui-ui/style/theme.hpp>
#include <imgui-ui/surface.hpp>
#include <imgui-ui/widgets/text.hpp>

#include <imgui.h>
#include <algorithm>

using namespace ui;

class NumberInputWidget::FieldNode final : public Widget {
public:
    explicit FieldNode(NumberInputWidget& owner) : Widget("input", "NumberInputField", InputMode::None), m_owner(owner) {
        set_size({grow(), grow()});
    }

private:
    void on_measure() override {
        ImGui::PushFont(font(), font_size());
        const float height = ImGui::GetTextLineHeight();
        ImGui::PopFont();
        set_measured_content_size({0.0F, height});
    }

    bool paint() override;

    NumberInputWidget& m_owner;
};

void NumberInputWidget::initialize_children() {
    set_size({grow(), fit()});
    m_label = &add<TextWidget>("");
    m_field = &add<FieldNode>(*this);
}

void NumberInputWidget::apply_theme_defaults(const Theme& theme) {
    Container::apply_theme_defaults(theme);
    const TransitionSpec transition{0.25F, easing::out_quad};
    set_font(surface().get_primary_font(), theme.controls.font_size);
    m_thumb_color = theme.controls.mark_color;
    m_thumb_size = theme.controls.thumb_size;
    m_label_spacing = {theme.metrics.item_inner_spacing.x, theme.metrics.item_spacing.y};
    update_label_layout();

    configure_all_styles([](Style& style) { style.padding({}).border(BorderEdge{0.0F, BorderType::None}); });
    m_field->configure_all_styles([&theme, transition](Style& style) { style.control(theme, transition); });

    m_field->style(StyleType::HOVER).background_color(theme.controls.hover_color, transition);
    m_field->style(StyleType::ACTIVE)
        .background_color(theme.controls.active_color, transition)
        .border_color(theme.accent_color, transition);
    m_field->style(StyleType::FOCUS).border_color(theme.accent_color);
    m_field->style(StyleType::DISABLED)
        .color(theme.text_secondary_color)
        .background_color(theme.background_secondary_color)
        .border_color(theme.border_color);
    style(StyleType::DISABLED).cursor(ImGuiMouseCursor_NotAllowed);
}

void NumberInputWidget::mouse_press_event(UiEvent& event) {
    if (event.button == PointerButton::Left) {
        surface().input_router().set_focus(this);
    }
}

NumberInputWidget& NumberInputWidget::set_label(std::string label, LabelPlacement placement) {
    m_label->set_text(std::move(label));
    m_label_placement = placement;
    update_label_layout();
    return *this;
}

void NumberInputWidget::update_label_layout() {
    const bool inline_label = m_label_placement == LabelPlacement::Inline;
    m_label->set_visible(!m_label->empty());
    set_direction(inline_label ? StackDirection::Horizontal : StackDirection::Vertical);
    set_content_alignment({0.0F, inline_label ? 0.5F : 0.0F});
    set_spacing(inline_label ? m_label_spacing.x : m_label_spacing.y);
}

void NumberInputWidget::value_changed() {
    notify_change();
}

Rect NumberInputWidget::hit_rect(Rect) const {
    return m_field->layout().visual_rect();
}

NumberInputWidget& NumberInputWidget::set_minimum(double minimum) {
    m_minimum = minimum;
    return *this;
}

NumberInputWidget& NumberInputWidget::set_maximum(double maximum) {
    m_maximum = maximum;
    return *this;
}

NumberInputWidget& NumberInputWidget::set_range(double minimum, double maximum) {
    m_minimum = std::min(minimum, maximum);
    m_maximum = std::max(minimum, maximum);
    return *this;
}

NumberInputWidget& NumberInputWidget::clear_range() {
    m_minimum.reset();
    m_maximum.reset();
    return *this;
}

NumberInputWidget& NumberInputWidget::set_speed(float speed) {
    m_speed = std::max(0.0F, speed);
    return *this;
}

NumberInputWidget& NumberInputWidget::set_format(std::string format) {
    m_format = std::move(format);
    return *this;
}

NumberInputWidget& NumberInputWidget::set_thumb_visible(bool visible) {
    m_thumb_visible = visible;
    return *this;
}

NumberInputWidget& NumberInputWidget::set_thumb_size(float size) {
    m_thumb_size = std::max(1.0F, size);
    return *this;
}

NumberInputWidget& NumberInputWidget::set_thumb_color(Color color) {
    m_thumb_color = std::move(color);
    return *this;
}

template <typename T>
constexpr ImGuiDataType number_data_type() {
    if constexpr (std::same_as<T, float>) {
        return ImGuiDataType_Float;
    } else if constexpr (std::same_as<T, double>) {
        return ImGuiDataType_Double;
    } else if constexpr (std::signed_integral<T>) {
        if constexpr (sizeof(T) == 1) return ImGuiDataType_S8;
        if constexpr (sizeof(T) == 2) return ImGuiDataType_S16;
        if constexpr (sizeof(T) == 4) return ImGuiDataType_S32;
        return ImGuiDataType_S64;
    } else {
        if constexpr (sizeof(T) == 1) return ImGuiDataType_U8;
        if constexpr (sizeof(T) == 2) return ImGuiDataType_U16;
        if constexpr (sizeof(T) == 4) return ImGuiDataType_U32;
        return ImGuiDataType_U64;
    }
}

template <typename T>
bool NumberInputWidget::draw_value(T& value) {
    constexpr ImGuiDataType data_type = number_data_type<T>();
    const T minimum = static_cast<T>(m_minimum.value_or(0.0));
    const T maximum = static_cast<T>(m_maximum.value_or(0.0));
    const void* minimum_ptr = m_minimum.has_value() ? &minimum : nullptr;
    const void* maximum_ptr = m_maximum.has_value() ? &maximum : nullptr;
    const char* format = m_format.empty() ? nullptr : m_format.c_str();

    if (m_thumb_visible && m_minimum.has_value() && m_maximum.has_value() && maximum > minimum) {
        return ImGui::SliderScalar("##value", data_type, &value, &minimum, &maximum, format);
    }

    return ImGui::DragScalar("##value", data_type, &value, m_speed, minimum_ptr, maximum_ptr, format);
}

bool NumberInputWidget::FieldNode::paint() {
    const ComputedStyle& current_style = computed_style();
    ImVec2 frame_padding = current_style.padding().window_padding();
    frame_padding.y = std::max(0.0F, (layout().size().y - ImGui::GetTextLineHeight()) * 0.5F);

    ImGui::PushID(this);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, frame_padding);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, m_owner.m_thumb_size);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, current_style.border_radius());
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, m_owner.m_thumb_color.rgba());
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, m_owner.m_thumb_color.rgba());

    ImGui::SetNextItemWidth(layout().size().x);

    if (std::visit([this](auto* value) { return m_owner.draw_value(*value); }, m_owner.m_number)) {
        m_owner.value_changed();
    }

    set_interaction_style(ImGui::IsItemHovered(), ImGui::IsItemActive(), m_owner.input_state().focused);
    painter().border({ImGui::GetItemRectMin(), ImGui::GetItemRectMax()}, current_style, current_style.border_color().value);

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    ImGui::PopID();

    return true;
}
