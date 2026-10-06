#include <imgui-ui/imgui/painter.hpp>
#include <imgui-ui/widgets/text-value.hpp>

#include <algorithm>
#include <cfloat>

using namespace ui;

void Painter::text(ImVec2 position, const Color& source, std::string_view text) const {
    m_draw_list.AddText(position, color(source), text.data(), text.data() + text.size());
}

void Painter::text(ImVec2 position, const Color& source, const GenericValue& text, const ImVec4* clip_rect) const {
    const ImColor draw_color = color(source);
    ImFont* font = text.font() != nullptr ? text.font() : ImGui::GetFont();
    const float font_size = ImGui::GetFontSize();
    const float wrap_width = std::max(0.0F, text.wrap_width());
    if (text.line_height_multiplier() == 1.0F) {
        m_draw_list.AddText(font, font_size, position, draw_color, text.c_str(), nullptr, wrap_width, clip_rect);
        return;
    }

    const char* const value = text.c_str();
    const char* const value_end = value + std::char_traits<char>::length(value);
    const float line_height = font_size * text.line_height_multiplier();
    float y = position.y;

    for (const char* paragraph = value;;) {
        const char* const paragraph_end = std::find(paragraph, value_end, '\n');
        const char* line = paragraph;

        do {
            const char* line_end = paragraph_end;
            if (wrap_width > 0.0F && line < paragraph_end) {
                line_end = font->CalcWordWrapPosition(font_size, line, paragraph_end, wrap_width);
                if (line_end == line) line_end = paragraph_end;
            }

            m_draw_list.AddText(font, font_size, {position.x, y}, draw_color, line, line_end, 0.0F, clip_rect);
            y += line_height;
            if (line_end == paragraph_end) break;

            line = line_end;
            while (line < paragraph_end && (*line == ' ' || *line == '\t')) {
                ++line;
            }
        } while (line < paragraph_end);

        if (paragraph_end == value_end) break;

        paragraph = paragraph_end + 1;
    }
}

void Painter::text_ellipsis(ImVec2 position, const Color& source, const GenericValue& text, ImVec4 clip_rect) const {
    const ImColor draw_color = color(source);
    ImFont* font = text.font() != nullptr ? text.font() : ImGui::GetFont();
    const float font_size = ImGui::GetFontSize();
    const char* const value = text.c_str();
    const char* const value_end = value + std::char_traits<char>::length(value);
    const float line_height = font_size * text.line_height_multiplier();
    float y = position.y;

    for (const char* line = value;;) {
        const char* const line_end = std::find(line, value_end, '\n');
        const float available_width = std::max(0.0F, clip_rect.z - position.x);
        const ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0F, line, line_end);
        if (text_size.x <= available_width) {
            m_draw_list.AddText(font, font_size, {position.x, y}, draw_color, line, line_end, 0.0F, &clip_rect);
        } else {
            constexpr char ellipsis[] = "...";
            const float ellipsis_width = font->CalcTextSizeA(font_size, FLT_MAX, 0.0F, ellipsis).x;
            const char* visible_end = line;
            const float text_width = std::max(0.0F, available_width - ellipsis_width);
            const ImVec2 visible_size = font->CalcTextSizeA(font_size, text_width, 0.0F, line, line_end, &visible_end);

            if (visible_end != line) {
                m_draw_list.AddText(font, font_size, {position.x, y}, draw_color, line, visible_end, 0.0F, &clip_rect);
            }

            m_draw_list.AddText(
                font, font_size, {position.x + visible_size.x, y}, draw_color, ellipsis, nullptr, 0.0F, &clip_rect
            );
        }

        if (line_end == value_end) break;

        line = line_end + 1;
        y += line_height;
    }
}
