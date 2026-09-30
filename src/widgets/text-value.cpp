#include <imgui-ui/widgets/text-value.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <type_traits>

using namespace ui;

const std::string& GenericValue::str() const {
    if (!m_string_dirty) return m_string;

    // convert the variant only after a text consumer asks for it, then reuse that string until the value changes.
    m_string = std::visit(
        [](const auto& value) -> std::string {
            using ValueType = std::remove_cvref_t<decltype(value)>;
            if constexpr (std::same_as<ValueType, std::string>) {
                return value;
            } else if constexpr (std::same_as<ValueType, bool>) {
                return value ? "true" : "false";
            } else {
                char buffer[64];
                const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), value);
                return error == std::errc{} ? std::string(buffer, end) : std::string{};
            }
        },
        m_value
    );
    m_string_dirty = false;
    return m_string;
}

void GenericValue::set_font(ImFont* font) {
    if (font == m_font) return;
    m_font = font;
    m_dirty = true;
}

void GenericValue::set_wrap(float wrap_width) {
    if (m_wrap_width == wrap_width) return;
    m_wrap_width = wrap_width;
    m_dirty = true;
}

void GenericValue::set_line_height(float multiplier) {
    const float resolved = std::max(0.0F, multiplier);
    if (m_line_height_multiplier == resolved) return;
    m_line_height_multiplier = resolved;
    m_dirty = true;
}

ImVec2 GenericValue::text_size() const {
    if (m_dirty) recompute();
    return m_text_size;
}

float GenericValue::line_height() const {
    if (m_dirty) recompute();
    return m_line_height;
}

bool GenericValue::set(std::string text) {
    const auto* current_text = std::get_if<std::string>(&m_value);
    if (current_text != nullptr && *current_text == text) return false;
    m_value = std::move(text);
    m_string_dirty = true;
    m_dirty = true;
    return true;
}

void GenericValue::recompute() const {
    if (ImGui::GetCurrentContext() == nullptr) {
        m_text_size = {};
        m_line_height = 0.0F;
        return;
    }

    ImFont* font = m_font != nullptr ? m_font : ImGui::GetFont();
    ImGui::PushFont(font);
    m_line_height = ImGui::GetTextLineHeight();
    m_text_size = ImGui::CalcTextSize(c_str(), nullptr, false, m_wrap_width);

    // imgui measures wrapped and explicit lines using native line height.
    // recover that line count before applying the configured height multiplier.
    if (m_line_height > 0.0F) {
        m_text_size.y = std::round(m_text_size.y / m_line_height) * m_line_height * m_line_height_multiplier;
    }
    ImGui::PopFont();
    m_dirty = false;
}
