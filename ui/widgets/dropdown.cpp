#include "dropdown.hpp"
#include "../layout/stack-container.hpp"
#include "../style/theme.hpp"
#include "../ui.hpp"
#include "../imgui/draw.hpp"
#include "text.hpp"

#include <imgui.h>
#include <algorithm>
#include <utility>

using namespace ui;

class ui::DropdownTriggerNode final : public DrawListWidget {
public:
    explicit DropdownTriggerNode(DropdownWidget::State& state) : DrawListWidget("trigger", "Dropdown"), m_state(state) {
        _on_event = [this](UiEvent& event) {
            if (event.type != EventType::Click || event.button != PointerButton::Left) {
                return;
            }

            if (m_state.is_closed()) {
                m_state.open();
            } else {
                m_state.close();
            }
        };
    }

private:
    void paint_draw_list(ImDrawList& draw_list, Rect rect, const Style& current_style) override {
        const auto selected = std::find_if(m_state.options.begin(), m_state.options.end(), [this](const DropdownOption& option) {
            return option.value == *m_state.value;
        });

        const std::string_view preview = selected == m_state.options.end() ? m_state.placeholder : selected->label;

        const ImColor background =
            m_state.is_open() ? style(StyleType::ACTIVE).background_color().value : current_style.background_color().value;
        draw_contents(draw_list, rect, preview, m_state.is_open(), current_style, background);
    }

    void draw_contents(
        ImDrawList& draw_list, Rect rect, std::string_view preview, bool open, const Style& current_style, ImColor background
    ) const {
        draw_frame(draw_list, rect, current_style, background);

        const ImVec2 text_size = ImGui::CalcTextSize(preview.data(), preview.data() + preview.size());

        draw_text(
            draw_list, {rect.min.x + current_style.padding().x, rect.min.y + (rect.size().y - text_size.y) * 0.5F},
            current_style.color().get_col(), preview
        );

        draw_triangle(
            draw_list, {rect.max.x - current_style.padding().x - 4.0F, rect.min.y + rect.size().y * 0.5F}, {8.0F, 4.0F},
            current_style.color().get_col(), open ? TriangleDirection::Up : TriangleDirection::Down
        );
    }

    DropdownWidget::State& m_state;
};

class ui::DropdownBodyNode final : public StackContainer {
public:
    DropdownBodyNode(InputRouter& router, DropdownWidget::State& state, const Widget& trigger)
        : StackContainer("body"), m_router(router), m_state(state), m_trigger(trigger) {
        set_type_name("DropdownBody");
        fade_out();
    }

    bool consume_change() {
        return std::exchange(m_changed, false);
    }

    void place_below(const Node& trigger) {
        const Rect rect = trigger.layout().visual_rect();
        m_popup_position = {rect.min.x, rect.max.y + 4.0F};
        m_popup_width = rect.size().x;
    }

    bool is_selected(std::string_view value) const {
        return *m_state.value == value;
    }

    void select(const std::string& value) {
        if (*m_state.value != value) {
            *m_state.value = value;
            m_changed = true;
            notify_change();
        }

        m_state.close();
    }

public:
    void draw() override {
        if (!m_state.is_open() && !m_state.is_closing()) {
            return;
        }

        if (m_state.is_open() && !m_popup_opened) {
            ImGui::OpenPopup("body");
            m_popup_opened = true;
        }

        const Style& current_style = style();
        const ImVec2 item_padding = {m_trigger.style().padding().x, 4.0F};
        const float item_height = ImGui::GetTextLineHeight() + item_padding.y * 2.0F;
        ImGui::SetNextWindowPos(m_popup_position, ImGuiCond_Always);
        ImGui::SetNextWindowSize({m_popup_width, item_height * static_cast<float>(m_state.options.size())});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{});
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, current_style.border_radius());
        ImGui::PushStyleColor(ImGuiCol_PopupBg, current_style.background_color().get_col());
        ImGui::PushStyleColor(ImGuiCol_Text, current_style.color().get_col());

        if (ImGui::BeginPopup("body", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
            const Rect body_rect = Rect::from_position_size(ImGui::GetWindowPos(), ImGui::GetWindowSize());
            set_visual_rect(body_rect);
            m_router.block(body_rect);

            const Style& hover_style = m_trigger.style(StyleType::HOVER);
            const ImColor selected_background = m_trigger.style(StyleType::ACTIVE).background_color().value;
            const ImColor hovered_background = hover_style.background_color().value;
            const ImColor hovered_text = hover_style.color().value;
            const ImGuiMouseCursor hover_cursor = hover_style.cursor();
            for (std::size_t index = 0; index < m_state.options.size(); ++index) {
                const DropdownOption& option = m_state.options[index];
                const ImVec2 item_position = ImGui::GetCursorScreenPos();

                ImGui::PushID(static_cast<int>(index));
                const bool pressed = ImGui::InvisibleButton("option", {m_popup_width, item_height});
                const bool selected = is_selected(option.value);
                const bool hovered = ImGui::IsItemHovered();
                ImGui::PopID();

                if (hovered) {
                    ImGui::SetMouseCursor(hover_cursor == ImGuiMouseCursor_None ? ImGuiMouseCursor_Arrow : hover_cursor);
                }

                if (selected || hovered) {
                    ImDrawFlags corners = ImDrawFlags_None;
                    if (index == 0) {
                        corners |= ImDrawFlags_RoundCornersTop;
                    }
                    if (index + 1 == m_state.options.size()) {
                        corners |= ImDrawFlags_RoundCornersBottom;
                    }
                    draw_rect_filled(
                        *ImGui::GetWindowDrawList(), Rect::from_position_size(item_position, {m_popup_width, item_height}),
                        hovered ? hovered_background : selected_background, current_style.border_radius(), corners
                    );
                }

                const ImVec2 text_size = ImGui::CalcTextSize(option.label.c_str());
                draw_text(
                    *ImGui::GetWindowDrawList(),
                    {item_position.x + item_padding.x, item_position.y + (item_height - text_size.y) * 0.5F},
                    hovered ? hovered_text : current_style.color().value, option.label
                );

                if (pressed) {
                    select(option.value);
                    ImGui::CloseCurrentPopup();
                    m_popup_opened = false;
                    break;
                }
            }

            ImGui::EndPopup();
        } else if (m_state.is_open()) {
            m_state.close();
            m_popup_opened = false;
        }

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
    }

protected:
    void on_update(float) override {
        if (!m_state.is_closing() || opacity() > VISIBILITY_OPACITY_THRESHOLD) {
            return;
        }

        m_state.finish_close();
        m_popup_opened = false;
    }

    InputRouter& m_router;
    DropdownWidget::State& m_state;
    const Widget& m_trigger;
    ImVec2 m_popup_position{};
    float m_popup_width = 0.0F;
    bool m_popup_opened = false;
    bool m_changed = false;
};

void DropdownWidget::State::open() {
    if (visibility != Visibility::Closed) {
        return;
    }

    visibility = Visibility::Open;
    if (body != nullptr) {
        body->set_enabled(true);
        body->fade_in();
    }
}

void DropdownWidget::State::close() {
    if (visibility != Visibility::Open) {
        return;
    }

    visibility = Visibility::Closing;
    if (body != nullptr) {
        body->set_enabled(false);
        body->fade_out();
    }
}

DropdownWidget::DropdownWidget(UI& ui, std::string& value, std::vector<DropdownOption> options, std::string id)
    : Widget(std::move(id), "Dropdown"), m_state{.value = &value, .options = std::move(options)} {
    m_label_node = &add<TextWidget>("");
    m_trigger = &add<DropdownTriggerNode>(m_state);
    m_body = &add<DropdownBodyNode>(ui.input_router(), m_state, *m_trigger);
    m_state.body = m_body;
    m_body->set_enabled(false);
    apply_theme_defaults(ui.theme());
}

void DropdownWidget::apply_theme_defaults(const Theme& theme) {
    const auto configure = [&theme](Widget& widget) {
        widget.configure_all_styles([&theme](Style& style) { style.control(theme).cursor(ImGuiMouseCursor_Hand); });
    };

    m_label_node->style().color(theme.text_color);

    configure(*m_trigger);
    m_body->configure_all_styles([&theme](Style& style) {
        style.color(theme.text_color)
            .background_color(theme.control_background_color)
            .padding({})
            .border_radius(theme.control_rounding);
    });

    m_trigger->configure_style(StyleType::HOVER, [&theme](Style& style) { style.background_color(theme.control_hover_color); });
    m_trigger->configure_style(StyleType::ACTIVE, [&theme](Style& style) { style.background_color(theme.control_active_color); });
}

DropdownWidget& DropdownWidget::set_label(std::string label) {
    m_label_node->set_text(std::move(label));
    return *this;
}

bool DropdownWidget::select_value(std::string_view value) {
    const auto option = std::find_if(m_state.options.begin(), m_state.options.end(), [value](const DropdownOption& candidate) {
        return candidate.value == value;
    });
    if (option == m_state.options.end() || m_state.value == nullptr || *m_state.value == option->value) {
        return false;
    }

    m_body->select(option->value);
    if (m_body->consume_change()) {
        notify_change();
    }
    return true;
}

DropdownWidget& DropdownWidget::set_placeholder(std::string placeholder) {
    if (m_state.placeholder == placeholder) {
        return *this;
    }

    m_state.placeholder = std::move(placeholder);
    return *this;
}

DropdownWidget& DropdownWidget::set_options(std::vector<DropdownOption> options) {
    if (m_state.options == options) {
        return *this;
    }

    m_state.options = std::move(options);
    return *this;
}

void DropdownWidget::on_measure() {
    ImVec2 size = layout().intrinsic_size();
    if (layout().size_spec().height.mode != LayoutSizeMode::Fixed) {
        size.y = ImGui::GetFrameHeight();
        if (has_label()) size.y += m_label_node->layout().size().y + ImGui::GetStyle().ItemSpacing.y;
    }

    set_measured_size(size, false, true);
}

Widget& DropdownWidget::trigger() {
    return *m_trigger;
}

Widget& DropdownWidget::body() {
    return *m_body;
}

void DropdownWidget::on_layout() {
    const float label_height = has_label() ? m_label_node->layout().size().y + ImGui::GetStyle().ItemSpacing.y : 0.0F;
    const ImVec2 outer_size = layout().size();
    const ImVec2 trigger_size = {
        outer_size.x,
        std::max(0.0F, outer_size.y - label_height),
    };

    m_trigger->set_size({px(trigger_size.x), px(trigger_size.y)});
}

void DropdownWidget::draw_children() {
    ImGui::PushID(this);
    const float content_x = ImGui::GetCursorPosX();

    if (has_label()) {
        m_label_node->draw();
        ImGui::SetCursorPosX(content_x);
    }

    m_trigger->draw();

    m_body->place_below(*m_trigger);
    m_body->draw();

    if (m_body->consume_change()) {
        notify_change();
    }
    ImGui::PopID();
}

bool DropdownWidget::has_label() const {
    return !m_label_node->empty();
}
