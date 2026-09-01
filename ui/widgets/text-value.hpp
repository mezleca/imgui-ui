#pragma once

#include "../imgui/draw.hpp"

#include <concepts>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <format>
#include <imgui.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace ui {
    template <typename T>
    concept GenericNumber = std::integral<T> || std::floating_point<T>;

    /// stores text or a numeric value and lazily caches its imgui font metrics.
    class GenericValue {
    public:
        // integers are normalized so every integer type is supported without expanding the variant.
        using Value = std::variant<bool, std::int64_t, std::uint64_t, float, double, std::string>;

        explicit GenericValue(std::string text = {}, ImFont* font = nullptr) : m_value(std::move(text)), m_font(font) {}
        template <typename T>
            requires GenericNumber<T>
        explicit GenericValue(T value, ImFont* font = nullptr) : m_font(font) {
            set(value);
        }

        const char* c_str() const {
            return str().c_str();
        }

        /// converts the value only when a consumer actually requests its string representation.
        const std::string& str() const {
            if (m_string_dirty) {
                m_string = std::visit(
                    [](const auto& value) -> std::string {
                        using ValueType = std::remove_cvref_t<decltype(value)>;
                        if constexpr (std::same_as<ValueType, std::string>) {
                            return value;
                        } else {
                            return std::format("{}", value);
                        }
                    },
                    m_value
                );
                m_string_dirty = false;
            }

            return m_string;
        }

        void set_font(ImFont* font) {
            if (font == m_font) {
                return;
            }

            m_font = font;
            m_dirty = true;
        }

        void set_wrap(float wrap_width) {
            if (m_wrap_width == wrap_width) {
                return;
            }

            m_wrap_width = wrap_width;
            m_dirty = true;
        }

        void set_line_height(float multiplier) {
            const float resolved = std::max(0.0F, multiplier);
            if (m_line_height_multiplier == resolved) {
                return;
            }

            m_line_height_multiplier = resolved;
            m_dirty = true;
        }

        ImVec2 text_size() const {
            if (m_dirty) {
                recompute();
            }

            return m_text_size;
        }

        float line_height() const {
            if (m_dirty) {
                recompute();
            }

            return m_line_height;
        }

        ImFont* font() const {
            return m_font;
        }

        float wrap_width() const {
            return m_wrap_width;
        }
        float line_height_multiplier() const {
            return m_line_height_multiplier;
        }

        template <typename T>
            requires GenericNumber<T>
        void set(T value) {
            Value new_value = make_value(value);

            if (new_value == m_value) {
                return;
            }

            m_value = std::move(new_value);
            m_string_dirty = true;
            m_dirty = true;
        }

        void set(std::string text) {
            const auto* current_text = std::get_if<std::string>(&m_value);
            if (current_text != nullptr && *current_text == text) {
                return;
            }

            m_value = std::move(text);
            m_string_dirty = true;
            m_dirty = true;
        }

    private:
        // convert every supported arithmetic category to one of the canonical storage types above.
        template <std::integral T>
        static Value make_value(T value) {
            if constexpr (std::same_as<T, bool>) {
                return value;
            } else if constexpr (std::signed_integral<T>) {
                return static_cast<std::int64_t>(value);
            } else {
                return static_cast<std::uint64_t>(value);
            }
        }

        template <std::floating_point T>
        static Value make_value(T value) {
            if constexpr (std::same_as<T, float>) {
                return value;
            } else {
                return static_cast<double>(value);
            }
        }

        // measures the current text and applies the configured line-height multiplier to its total height.
        void recompute() const {
            if (ImGui::GetCurrentContext() == nullptr) {
                m_text_size = {};
                m_line_height = 0.0F;
                return;
            }

            ImFont* font = m_font != nullptr ? m_font : ImGui::GetFont();
            ImGui::PushFont(font);

            m_line_height = ImGui::GetTextLineHeight();
            m_text_size = ImGui::CalcTextSize(c_str(), nullptr, false, m_wrap_width);

            // imgui measured wrapped and explicit lines with its native height; recover that line count before scaling it.
            if (m_line_height > 0.0F) {
                m_text_size.y = std::round(m_text_size.y / m_line_height) * m_line_height * m_line_height_multiplier;
            }

            ImGui::PopFont();
            m_dirty = false;
        }

        Value m_value;
        mutable std::string m_string;
        ImFont* m_font = nullptr;
        mutable ImVec2 m_text_size;
        mutable float m_line_height = 0.0F;
        float m_line_height_multiplier = 1.0F;
        float m_wrap_width = -1.0F;
        mutable bool m_string_dirty = true;
        mutable bool m_dirty = true;
    };

} // namespace ui
