#pragma once

#include <concepts>
#include <cstdint>
#include <format>
#include <imgui.h>
#include <string>
#include <tuple>
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
        virtual ~GenericValue() = default;

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

        void set_wrap(float wrap_end) {
            if (m_wrap_end == wrap_end) {
                return;
            }

            m_wrap_end = wrap_end;
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

        // recomputes values for size and line height.
        void recompute() const {
            if (ImGui::GetCurrentContext() == nullptr) {
                m_text_size = {};
                m_line_height = 0.0F;
                return;
            }

            ImFont* font = m_font != nullptr ? m_font : ImGui::GetFont();
            ImGui::PushFont(font);

            m_line_height = ImGui::GetTextLineHeight();
            m_text_size = ImGui::CalcTextSize(c_str(), nullptr, false, m_wrap_end);

            ImGui::PopFont();
            m_dirty = false;
        }

        Value m_value;
        mutable std::string m_string;
        ImFont* m_font = nullptr;
        mutable ImVec2 m_text_size;
        mutable float m_line_height = 0.0F;
        float m_wrap_end = -1.0f;
        mutable bool m_string_dirty = true;
        mutable bool m_dirty = true;
    };

    /// recomputes its string only when the bound tuple changes.
    template <typename... Args>
    class TextFormatted : public GenericValue {
    public:
        explicit TextFormatted(std::string fmt, ImFont* font = nullptr)
            : GenericValue(std::string{}, font), m_fmt(std::move(fmt)) {}

        void set(std::tuple<Args...> new_values) {
            if (new_values == m_values) {
                return;
            }

            m_values = std::move(new_values);
            recompute_text();
        }

    private:
        void recompute_text() {
            GenericValue::set(
                std::apply([this](auto const&... vals) { return std::vformat(m_fmt, std::make_format_args(vals...)); }, m_values)
            );
        }

        std::string m_fmt;
        std::tuple<Args...> m_values = {};
    };

} // namespace ui
