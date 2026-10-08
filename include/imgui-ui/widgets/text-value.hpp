#pragma once

#include <concepts>
#include <cstdint>
#include <imgui.h>
#include <string>
#include <utility>
#include <variant>

namespace ui {
    template <typename T>
    concept GenericNumber = std::integral<T> || std::floating_point<T>;

    class GenericValue {
    public:
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
        const std::string& str() const;

        void set_font(ImFont* font, float size = 0.0F);

        void set_wrap(float wrap_width);

        void set_line_height(float multiplier);

        ImVec2 text_size() const;

        float line_height() const;

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
        bool set(T value) {
            Value new_value;
            if constexpr (std::same_as<T, bool> || std::same_as<T, float>) {
                new_value = value;
            } else if constexpr (std::signed_integral<T>) {
                new_value = static_cast<std::int64_t>(value);
            } else if constexpr (std::unsigned_integral<T>) {
                new_value = static_cast<std::uint64_t>(value);
            } else {
                new_value = static_cast<double>(value);
            }

            if (new_value == m_value) {
                return false;
            }

            m_value = std::move(new_value);
            m_string_dirty = true;
            m_dirty = true;
            return true;
        }

        bool set(std::string text);

    private:
        void recompute() const;

        Value m_value;
        mutable std::string m_string;
        ImFont* m_font = nullptr;
        float m_font_size = 0.0F;
        mutable ImVec2 m_text_size;
        mutable float m_line_height = 0.0F;
        float m_line_height_multiplier = 1.0F;
        float m_wrap_width = -1.0F;
        mutable bool m_string_dirty = true;
        mutable bool m_dirty = true;
    };

} // namespace ui
