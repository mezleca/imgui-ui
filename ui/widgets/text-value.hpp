#pragma once

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

        void draw(ImDrawList& draw_list, ImVec2 position, ImU32 color, const ImVec4* clip_rect = nullptr) const {
            ImFont* current_font = m_font != nullptr ? m_font : ImGui::GetFont();
            const float font_size = ImGui::GetFontSize();
            if (m_line_height_multiplier == 1.0F) {
                draw_list.AddText(
                    current_font, font_size, position, color, c_str(), nullptr, m_wrap_width >= 0.0F ? m_wrap_width : 0.0F,
                    clip_rect
                );
                return;
            }

            // addtext has no line-height parameter, so each wrapped line is emitted at the configured vertical advance.
            const char* const text = c_str();
            const char* const text_end = text + std::char_traits<char>::length(text);
            const float wrap_width = m_wrap_width >= 0.0F ? m_wrap_width : 0.0F;
            const float line_height = font_size * m_line_height_multiplier;
            float y = position.y;

            for (const char* paragraph = text;;) {
                const char* const paragraph_end = std::find(paragraph, text_end, '\n');
                const char* line = paragraph;

                do {
                    const char* line_end = paragraph_end;
                    if (wrap_width > 0.0F && line < paragraph_end) {
                        line_end = current_font->CalcWordWrapPosition(font_size, line, paragraph_end, wrap_width);
                        if (line_end == line) {
                            line_end = paragraph_end;
                        }
                    }

                    draw_list.AddText(current_font, font_size, {position.x, y}, color, line, line_end, 0.0F, clip_rect);
                    y += line_height;

                    if (line_end == paragraph_end) {
                        break;
                    }

                    line = line_end;
                    while (line < paragraph_end && (*line == ' ' || *line == '\t')) {
                        ++line;
                    }
                } while (line < paragraph_end);

                if (paragraph_end == text_end) {
                    break;
                }

                paragraph = paragraph_end + 1;
            }
        }

        void draw_ellipsis(ImDrawList& draw_list, ImVec2 position, ImU32 color, ImVec4 clip_rect) const {
            ImFont* current_font = m_font != nullptr ? m_font : ImGui::GetFont();
            const float font_size = ImGui::GetFontSize();
            const char* const text = c_str();
            const char* const text_end = text + std::char_traits<char>::length(text);
            const float line_height = font_size * m_line_height_multiplier;
            float y = position.y;

            for (const char* line = text;;) {
                const char* const line_end = std::find(line, text_end, '\n');
                draw_ellipsis_line(draw_list, current_font, font_size, {position.x, y}, color, line, line_end, clip_rect);

                if (line_end == text_end) {
                    break;
                }

                line = line_end + 1;
                y += line_height;
            }
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
        static void draw_ellipsis_line(
            ImDrawList& draw_list, ImFont* font, float font_size, ImVec2 position, ImU32 color, const char* text,
            const char* text_end, ImVec4 clip_rect
        ) {
            const float available_width = std::max(0.0F, clip_rect.z - position.x);
            const ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0F, text, text_end);
            if (text_size.x <= available_width) {
                draw_list.AddText(font, font_size, position, color, text, text_end, 0.0F, &clip_rect);
                return;
            }

            constexpr std::string_view ellipsis = "...";
            const float ellipsis_width = font->CalcTextSizeA(font_size, FLT_MAX, 0.0F, ellipsis.data()).x;
            const char* visible_end = text;
            const float text_width = std::max(0.0F, available_width - ellipsis_width);
            const ImVec2 visible_size = font->CalcTextSizeA(font_size, text_width, 0.0F, text, text_end, &visible_end);

            if (visible_end != text) {
                draw_list.AddText(font, font_size, position, color, text, visible_end, 0.0F, &clip_rect);
            }

            draw_list.AddText(
                font, font_size, {position.x + visible_size.x, position.y}, color, ellipsis.data(), nullptr, 0.0F, &clip_rect
            );
        }

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
