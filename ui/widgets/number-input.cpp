#include "number-input.hpp"
#include "../imgui/draw.hpp"
#include "../style/theme.hpp"
#include "../ui.hpp"

#include <imgui.h>
#include <algorithm>

using namespace ui;

void NumberInputWidget::initialize(UI& ui) {
    apply_theme_defaults(ui.theme());
}

void NumberInputWidget::apply_theme_defaults(const Theme& theme) {
    m_thumb_color = theme.control_mark_color;
    m_thumb_size = theme.control_thumb_size;

    configure_all_styles([&theme](Style& style) { style.control(theme); });

    configure_style(StyleType::HOVER, [&theme](Style& style) {
        style.background_color(theme.control_hover_color).border_color(theme.accent_hover_color);
    });

    configure_style(StyleType::ACTIVE, [&theme](Style& style) {
        style.background_color(theme.control_active_color).border_color(theme.accent_color);
    });
}

NumberInputWidget& NumberInputWidget::set_label(std::string label) {
    if (label == m_label.str()) {
        return *this;
    }

    m_label.set(std::move(label));
    invalidate_measure();
    return *this;
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

NumberInputWidget& NumberInputWidget::set_thumb_color(ImColor color) {
    m_thumb_color = color;
    return *this;
}

void NumberInputWidget::sync_value() const {
    std::visit([this](const auto* value) { m_value.set(*value); }, m_number);
}

void NumberInputWidget::on_measure() {
    ImVec2 size = layout().intrinsic_size();
    const ImVec2 padding = style().padding();
    sync_value();
    m_value.set_font(font());
    m_label.set_font(font());

    if (layout().size_spec().height.mode != LayoutSizeMode::Fixed) {
        size.y = m_value.line_height() + padding.y * 2.0F;
    }

    set_measured_size(size, false, true);
}

template <typename T>
constexpr ImGuiDataType number_data_type() {
    // imgui requires the scalar type to match the bound value.
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

bool NumberInputWidget::paint() {
    const Style& current_style = style();
    ImVec2 frame_padding = current_style.padding();
    m_label.set_font(font());
    const ImVec2 label_size = m_label.text_size();

    if (layout().size().y > 0.0F) {
        frame_padding.y = std::max(0.0F, (layout().size().y - ImGui::GetTextLineHeight()) * 0.5F);
    }

    ImGui::PushID(this);
    ImGui::BeginGroup();

    float input_width = layout().size().x;
    if (label_size.x > 0.0F) {
        const float label_width = label_size.x + ImGui::GetStyle().ItemInnerSpacing.x;
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(m_label.c_str());
        ImGui::SameLine();
        if (input_width > 0.0F) {
            input_width = std::max(1.0F, input_width - label_width);
        }
    }

    ImGui::SetNextItemWidth(input_width > 0.0F ? input_width : -1.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, frame_padding);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, m_thumb_size);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, current_style.border_radius());
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, m_thumb_color.Value);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, m_thumb_color.Value);

    if (std::visit([this](auto* value) { return draw_value(*value); }, m_number)) {
        notify_change();
    }

    draw_border({ImGui::GetItemRectMin(), ImGui::GetItemRectMax()}, current_style);

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    ImGui::EndGroup();
    ImGui::PopID();

    return true;
}
