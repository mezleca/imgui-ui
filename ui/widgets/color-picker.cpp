#include "color-picker.hpp"

#include "../imgui/draw.hpp"
#include "../style/theme.hpp"
#include "../ui.hpp"
#include "text-input.hpp"
#include "text.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <imgui.h>
#include <string>
#include <utility>

using namespace ui;

static float saturate(float value) {
    return std::clamp(value, 0.0F, 1.0F);
}

static bool equal_color(ImColor left, ImColor right) {
    return left.Value.x == right.Value.x && left.Value.y == right.Value.y && left.Value.z == right.Value.z &&
           left.Value.w == right.Value.w;
}

static std::string format_hex(ImColor color) {
    return std::format(
        "#{:02X}{:02X}{:02X}{:02X}", static_cast<unsigned int>(saturate(color.Value.x) * 255.0F),
        static_cast<unsigned int>(saturate(color.Value.y) * 255.0F), static_cast<unsigned int>(saturate(color.Value.z) * 255.0F),
        static_cast<unsigned int>(saturate(color.Value.w) * 255.0F)
    );
}

static int hex_digit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

static bool parse_hex(const std::string& text, ImColor& color) {
    const std::size_t offset = !text.empty() && text[0] == '#' ? 1 : 0;
    const std::size_t length = text.size() - offset;
    if (length != 6 && length != 8) {
        return false;
    }

    std::array<unsigned char, 4> channels{0, 0, 0, 255};
    for (std::size_t index = 0; index < length / 2; ++index) {
        const int high = hex_digit(text[offset + index * 2]);
        const int low = hex_digit(text[offset + index * 2 + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        channels[index] = static_cast<unsigned char>((high << 4) | low);
    }

    color = ImColor(channels[0], channels[1], channels[2], channels[3]);
    return true;
}

static void draw_checkerboard(ImDrawList& draw_list, Rect rect, float cell_size, ImColor light, ImColor dark) {
    cell_size = std::max(1.0F, cell_size);

    for (float y = rect.min.y; y < rect.max.y; y += cell_size) {
        const int row = static_cast<int>((y - rect.min.y) / cell_size);
        for (float x = rect.min.x; x < rect.max.x; x += cell_size) {
            const int column = static_cast<int>((x - rect.min.x) / cell_size);
            const Rect cell =
                Rect::from_position_size({x, y}, {std::min(cell_size, rect.max.x - x), std::min(cell_size, rect.max.y - y)});
            draw_rect_filled(draw_list, cell, (row + column) % 2 == 0 ? light : dark);
        }
    }
}

static ImColor hsv_color(float hue, float saturation, float value, float alpha = 1.0F) {
    ImColor color;
    ImGui::ColorConvertHSVtoRGB(hue, saturation, value, color.Value.x, color.Value.y, color.Value.z);
    color.Value.w = alpha;
    return color;
}

class ui::ColorPickerPreviewNode final : public DrawListWidget {
public:
    ColorPickerPreviewNode(ColorPickerWidget& owner, ImColor& color)
        : DrawListWidget("preview", "ColorPickerPreview"), m_owner(owner), m_color(&color) {}

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

        if (m_owner.is_open()) {
            m_owner.close();
        } else {
            m_owner.open();
        }
    }

    void input_state_changed() override {
        StyledNode::input_state_changed();
        if (m_owner.is_open()) set_visual_style(StyleType::ACTIVE);
    }

    void paint_draw_list(ImDrawList& draw_list, Rect rect, const ComputedStyle& style) override {
        draw_frame(draw_list, rect, style, *m_color);
    }

    ColorPickerWidget& m_owner;
    ImColor* m_color = nullptr;
};

class ui::ColorPickerPopup final : public Widget {
public:
    ColorPickerPopup(ColorPickerWidget& owner, UI& ui)
        : Widget("popup", "ColorPickerPopup", InputMode::None), m_owner(owner), m_ui(ui),
          m_window_name(std::format("##color-picker-{}", identity())) {
        set_layout({.in_flow = false});

        m_hex_input = &add<TextInputWidget>(ui, m_hex, "hex");
        const Theme& theme = ui.theme();
        m_hex_input->configure_all_styles([&theme](Style& style) {
            style.color(theme.text_color)
                .background_color(theme.background_tertiary_color)
                .border(BORDER_ALL)
                .border_color(theme.controls.border_color)
                .border_radius(theme.controls.rounding)
                .border_thickness(theme.controls.border_thickness)
                .padding(theme.metrics.item_inner_spacing);
        });
        m_hex_input->configure_style(StyleType::HOVER, [&theme](Style& style) { style.border_color(theme.accent_hover_color); });
        m_hex_input->configure_style(StyleType::ACTIVE, [&theme](Style& style) { style.border_color(theme.accent_color); });
        m_hex_input->configure_style(StyleType::FOCUS, [&theme](Style& style) { style.border_color(theme.accent_color); });
        m_hex_input->set_on_change([this] {
            ImColor parsed;
            if (parse_hex(m_hex, parsed)) {
                m_owner.set_color(parsed);
            }
        });

        apply_theme_defaults(ui.theme());
    }

    void show() {
        m_hex = format_hex(*m_owner.m_color);
    }

private:
    void apply_theme_defaults(const Theme& theme) override {
        const ImVec2 padding = {theme.content_padding, theme.content_padding};
        const float bar_size = std::max(1.0F, theme.controls.thumb_size);

        set_size(
            {px(theme.widgets.color_picker_selector_size + theme.metrics.item_spacing.x + bar_size + padding.x * 2.0F), fit()}
        );

        configure_all_styles([&theme](Style& style) {
            style.color(theme.text_color)
                .background_color(theme.controls.background_color)
                .border(BORDER_ALL)
                .border_color(theme.controls.border_color)
                .border_radius(theme.metrics.popup_rounding)
                .border_thickness(theme.controls.border_thickness)
                .padding({theme.content_padding, theme.content_padding});
        });
    }

    bool paint() override {
        if (!m_owner.is_open()) {
            return false;
        }

        const Rect preview_rect = m_owner.preview().layout().visual_rect();
        const ImVec2 position = {preview_rect.min.x, preview_rect.max.y + m_ui.theme().metrics.item_spacing.y};
        const ComputedStyle& style = computed_style();

        ImGui::SetNextWindowPos(position, ImGuiCond_Always);
        ImGui::SetNextWindowSize({layout().size().x, 0.0F}, ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.padding());
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, style.border_radius());
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4{});

        // a regular window leaves an existing root popup open when this picker opens from the debugger.
        const bool draw_content = ImGui::Begin(
            m_window_name.c_str(), nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
        );

        if (draw_content) {
            const Rect popup_rect = Rect::from_position_size(ImGui::GetWindowPos(), ImGui::GetWindowSize());

            set_visual_rect(popup_rect);
            draw_frame(*ImGui::GetWindowDrawList(), popup_rect, style);

            m_ui.input_router().register_target(*this, popup_rect);

            // block the whole work area so an empty outside press can close the popup without click synthesis.
            m_ui.input_router().register_blocker(*this, viewport_work_area(), [this, popup_rect](UiEvent& event) {
                if (event.type == EventType::PointerDown && event.button == PointerButton::Left &&
                    !popup_rect.contains(event.position)) {
                    m_owner.close();
                }
            });

            draw_picker();
        }

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
        return draw_content;
    }

    void draw_picker() {
        const Theme& theme = m_ui.theme();
        const ImVec2 spacing = theme.metrics.item_spacing;
        const float bar_size = std::max(1.0F, theme.controls.thumb_size);

        ImColor color = *m_owner.m_color;
        float hue = 0.0F;
        float saturation = 0.0F;
        float value = 0.0F;

        ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, hue, saturation, value);

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const float selector_size = std::max(1.0F, ImGui::GetContentRegionAvail().x - spacing.x - bar_size);
        const Rect selector = Rect::from_position_size(origin, {selector_size, selector_size});
        const Rect hue_bar = Rect::from_position_size({selector.max.x + spacing.x, selector.min.y}, {bar_size, selector_size});
        const Rect alpha_bar =
            Rect::from_position_size({selector.min.x, selector.max.y + spacing.y}, {hue_bar.max.x - selector.min.x, bar_size});

        // invisible controls update hsv before the surfaces below read the new color for this frame.
        ImGui::InvisibleButton("selector", selector.size());
        if (ImGui::IsItemActive()) {
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            saturation = saturate((mouse.x - selector.min.x) / selector.size().x);
            value = 1.0F - saturate((mouse.y - selector.min.y) / selector.size().y);
            set_hsv(hue, saturation, value, color.Value.w);
            color = *m_owner.m_color;
        }

        ImGui::SetCursorScreenPos(hue_bar.min);
        ImGui::InvisibleButton("hue", hue_bar.size());
        if (ImGui::IsItemActive()) {
            hue = saturate((ImGui::GetIO().MousePos.y - hue_bar.min.y) / hue_bar.size().y);
            set_hsv(hue, saturation, value, color.Value.w);
            color = *m_owner.m_color;
        }

        ImGui::SetCursorScreenPos(alpha_bar.min);
        ImGui::InvisibleButton("alpha", alpha_bar.size());
        if (ImGui::IsItemActive()) {
            color.Value.w = saturate((ImGui::GetIO().MousePos.x - alpha_bar.min.x) / alpha_bar.size().x);
            m_owner.set_color(color);
            m_hex = format_hex(color);
        }

        ImDrawList& draw_list = ui::draw_list();
        const ImColor hue_color = hsv_color(hue, 1.0F, 1.0F);
        draw_rect_filled_gradient(draw_list, selector, ImColor{IM_COL32_WHITE}, hue_color, hue_color, ImColor{IM_COL32_WHITE});
        draw_rect_filled_gradient(
            draw_list, selector, ImColor{0, 0, 0, 0}, ImColor{0, 0, 0, 0}, ImColor{0, 0, 0, 255}, ImColor{0, 0, 0, 255}
        );
        draw_rect_outline(draw_list, selector, theme.controls.border_color);

        for (int index = 0; index < 6; ++index) {
            const float top = hue_bar.min.y + hue_bar.size().y * static_cast<float>(index) / 6.0F;
            const float bottom = hue_bar.min.y + hue_bar.size().y * static_cast<float>(index + 1) / 6.0F;
            const ImColor first = hsv_color(static_cast<float>(index) / 6.0F, 1.0F, 1.0F);
            const ImColor second = hsv_color(static_cast<float>(index + 1) / 6.0F, 1.0F, 1.0F);
            draw_rect_filled_gradient(draw_list, {{hue_bar.min.x, top}, {hue_bar.max.x, bottom}}, first, first, second, second);
        }
        draw_rect_outline(draw_list, hue_bar, theme.controls.border_color);

        draw_checkerboard(
            draw_list, alpha_bar, bar_size * 0.5F, theme.background_secondary_color, theme.background_tertiary_color
        );

        const ImColor opaque = ImColor(color.Value.x, color.Value.y, color.Value.z, 1.0F);
        const ImColor transparent = ImColor(color.Value.x, color.Value.y, color.Value.z, 0.0F);

        draw_rect_filled_gradient(draw_list, alpha_bar, transparent, opaque, opaque, transparent);
        draw_rect_outline(draw_list, alpha_bar, theme.controls.border_color);

        const ImVec2 selector_cursor = {
            selector.min.x + saturation * selector.size().x,
            selector.min.y + (1.0F - value) * selector.size().y,
        };

        draw_circle(draw_list, selector_cursor, 5.0F, theme.background_color);
        draw_circle_outline(draw_list, selector_cursor, 5.0F, theme.text_color, 1.5F);

        const float hue_cursor_y = hue_bar.min.y + hue * hue_bar.size().y;

        draw_line(draw_list, {hue_bar.min.x - 2.0F, hue_cursor_y}, {hue_bar.max.x + 2.0F, hue_cursor_y}, theme.text_color, 2.0F);
        draw_line(
            draw_list, {hue_bar.min.x - 2.0F, hue_cursor_y + 1.0F}, {hue_bar.max.x + 2.0F, hue_cursor_y + 1.0F},
            theme.background_color, 1.0F
        );

        const float alpha_cursor_x = alpha_bar.min.x + color.Value.w * alpha_bar.size().x;

        draw_line(
            draw_list, {alpha_cursor_x, alpha_bar.min.y - 2.0F}, {alpha_cursor_x, alpha_bar.max.y + 2.0F}, theme.text_color, 2.0F
        );

        draw_line(
            draw_list, {alpha_cursor_x + 1.0F, alpha_bar.min.y - 2.0F}, {alpha_cursor_x + 1.0F, alpha_bar.max.y + 2.0F},
            theme.background_color, 1.0F
        );

        ImGui::SetCursorScreenPos({alpha_bar.min.x, alpha_bar.max.y + spacing.y});

        m_hex_input->set_size({px(alpha_bar.size().x), fit()});
        m_hex_input->draw();
    }

    void set_hsv(float hue, float saturation, float value, float alpha) {
        m_owner.set_color(hsv_color(hue, saturation, value, alpha));
        m_hex = format_hex(*m_owner.m_color);
    }

    void draw_children() override {}

    ColorPickerWidget& m_owner;
    UI& m_ui;
    std::string m_window_name;
    std::string m_hex;
    TextInputWidget* m_hex_input = nullptr;
};

ColorPickerWidget::ColorPickerWidget(UI& ui, ImColor& color, std::string label, std::string id)
    : StackContainer(std::move(id), StackDirection::Vertical), m_color(&color) {
    m_label_node = &add<TextWidget>(std::move(label));
    m_label_node->set_visible(!m_label_node->empty());
    m_preview = &add<ColorPickerPreviewNode>(*this, color);
    m_popup = &add<ColorPickerPopup>(*this, ui);

    apply_theme_defaults(ui.theme());
}

ColorPickerWidget& ColorPickerWidget::set_label(std::string label) {
    m_label_node->set_text(std::move(label));
    m_label_node->set_visible(!m_label_node->empty());
    return *this;
}

void ColorPickerWidget::apply_theme_defaults(const Theme& theme) {
    set_spacing(theme.metrics.item_spacing.y);

    const ImVec2 preview_size = {
        theme.controls.thumb_size * 2.0F + theme.metrics.frame_padding.x * 2.0F,
        theme.controls.thumb_size + theme.metrics.frame_padding.y * 2.0F,
    };

    set_size({px(preview_size.x), fit()});
    m_preview->set_size({px(preview_size.x), px(preview_size.y)});

    configure_all_styles([](Style& style) { style.padding({}); });

    m_label_node->configure_all_styles([&theme](Style& style) { style.color(theme.text_color); });
    m_preview->configure_all_styles([&theme](Style& style) {
        style.background_color(theme.controls.background_color)
            .border(BORDER_ALL)
            .border_color(theme.controls.border_color)
            .border_radius(theme.controls.rounding)
            .border_thickness(theme.controls.border_thickness)
            .padding({})
            .cursor(ImGuiMouseCursor_Hand);
    });

    m_preview->configure_style(StyleType::HOVER, [&theme](Style& style) { style.border_color(theme.accent_hover_color); });
    m_preview->configure_style(StyleType::ACTIVE, [&theme](Style& style) { style.border_color(theme.accent_color); });
}

bool ColorPickerWidget::set_color(ImColor color) {
    if (equal_color(*m_color, color)) {
        return false;
    }

    *m_color = color;
    notify_change();
    return true;
}

void ColorPickerWidget::open() {
    if (m_open) {
        return;
    }

    m_open = true;
    m_popup->show();
    m_preview->set_open(true);
}

void ColorPickerWidget::close() {
    m_open = false;
    m_preview->set_open(false);
}

TextWidget& ColorPickerWidget::label() {
    return *m_label_node;
}

Widget& ColorPickerWidget::preview() {
    return *m_preview;
}

Widget& ColorPickerWidget::popup() {
    return *m_popup;
}
