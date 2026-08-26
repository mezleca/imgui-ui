#include "debugger.hpp"
#include "backend.hpp"
#include "../../ui.hpp"
#include "../../style/styled-node.hpp"
#include "../../style/theme.hpp"
#include "../../imgui/context-scope.hpp"
#include "../../imgui/draw.hpp"

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_log.h>
#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

namespace ui {
    static constexpr const char* ALIGNMENT_NAMES[] = {
        "top-left",     "top-center",  "top-right",     "center-left",  "center",
        "center-right", "bottom-left", "bottom-center", "bottom-right", "custom",
    };

    static constexpr const char* STYLE_NAMES[] = {"default", "hover", "active", "focus"};

    static constexpr ImVec2 ICON_SIZE = {16.0F, 16.0F};
    static constexpr float WINDOW_PADDING = 8.0F;
    static constexpr float ITEM_SPACING = 6.0F;
    static constexpr float INPUT_MAX_WIDTH = 180.0F;
    static constexpr ImVec2 INPUT_PADDING = {0.0F, 0.0F};
    static constexpr ImVec2 SECTION_PADDING = {10.0F, 6.0F};
    static bool property_section_open = false;
    static bool property_section_indented = false;

    static const char* input_layer_name(InputLayer layer) {
        switch (layer) {
            case InputLayer::Content:
                return "content";
            case InputLayer::Overlay:
                return "overlay";
            case InputLayer::Modal:
                return "modal";
            case InputLayer::Notification:
                return "notification";
            case InputLayer::Count:
                return "unassigned";
        }

        return "unknown";
    }

    static void draw_property_value(std::string_view label, const char* format, ...) {
        char value[256];
        va_list args;
        va_start(args, format);
        std::vsnprintf(value, sizeof(value), format, args);
        va_end(args);

        ImGui::Text("%.*s:", static_cast<int>(label.size()), label.data());
        ImGui::SameLine(0.0F, ITEM_SPACING);
        ImGui::TextDisabled("%s", value);
    }

    static void end_property_section() {
        if (!property_section_open) {
            return;
        }

        if (property_section_indented) {
            ImGui::Unindent(SECTION_PADDING.x);
            ImGui::Dummy({0.0F, SECTION_PADDING.y});
            property_section_indented = false;
        }

        ImGui::EndChild();
        property_section_open = false;
    }

    static void draw_property_section(std::string_view label) {
        end_property_section();

        ImGui::Spacing();

        const std::string id{label};
        const ImVec4 section_color = ImGui::GetStyle().Colors[ImGuiCol_FrameBg];
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0F, 0.0F});
        ImGui::PushStyleColor(ImGuiCol_ChildBg, section_color);
        ImGui::BeginChild(
            id.c_str(), {0.0F, 0.0F}, ImGuiChildFlags_AutoResizeY,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
        );
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        property_section_open = true;

        const ImVec2 item_spacing = ImGui::GetStyle().ItemSpacing;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {item_spacing.x, 2.0F});
        ImGui::Indent(SECTION_PADDING.x);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_CheckMark]);
        ImGui::TextUnformatted(label.data(), label.data() + label.size());
        ImGui::PopStyleColor();
        ImGui::Unindent(SECTION_PADDING.x);
        ImGui::Separator();
        ImGui::PopStyleVar();
        ImGui::Indent(SECTION_PADDING.x);
        property_section_indented = true;
    }

    static void draw_rect_properties(std::string_view label, Rect rect) {
        const ImVec2 size = rect.size();
        draw_property_value(label, "position x %.1f, y %.1f; size w %.1f, h %.1f", rect.min.x, rect.min.y, size.x, size.y);
    }

    template <typename DrawInput>
    static bool draw_labeled_input(std::string_view label, DrawInput draw_input, ImVec4 frame_background = {}) {
        if (frame_background.w == 0.0F) {
            frame_background = ImGui::GetStyle().Colors[ImGuiCol_FrameBg];
            frame_background.x *= 0.72F;
            frame_background.y *= 0.72F;
            frame_background.z *= 0.72F;
        }

        const std::string id{label};

        ImGui::PushID(id.c_str());
        const ImVec4 transparent = {0.0F, 0.0F, 0.0F, 0.0F};
        ImGui::PushStyleColor(ImGuiCol_FrameBg, frame_background);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, frame_background);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, frame_background);
        ImGui::PushStyleColor(ImGuiCol_Border, transparent);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, INPUT_PADDING);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0F);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%.*s:", static_cast<int>(label.size()), label.data());
        ImGui::SameLine(0.0F, ITEM_SPACING);
        const float input_width = std::min(INPUT_MAX_WIDTH, std::max(0.0F, ImGui::GetContentRegionAvail().x));
        ImGui::SetNextItemWidth(input_width);
        const bool changed = draw_input();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(5);
        ImGui::PopID();
        return changed;
    }

    static bool draw_text_input(std::string_view label, std::string& value) {
        return draw_labeled_input(label, [&value] { return ImGui::InputText("##value", &value); });
    }

    static bool draw_color_input(std::string_view label, ImVec4& value) {
        return draw_labeled_input(label, [&value] {
            return ImGui::ColorEdit4("##value", &value.x, ImGuiColorEditFlags_NoInputs);
        });
    }

    static bool draw_inline_combo(std::string_view label, int* selected, const char* const items[], int item_count) {
        return draw_labeled_input(label, [selected, items, item_count] {
            const bool valid_selection = selected != nullptr && *selected >= 0 && *selected < item_count;
            const char* preview = valid_selection ? items[*selected] : "select";
            bool changed = false;

            if (ImGui::BeginCombo("##value", preview, ImGuiComboFlags_NoArrowButton)) {
                for (int index = 0; index < item_count; ++index) {
                    const bool is_selected = selected != nullptr && *selected == index;
                    if (ImGui::Selectable(items[index], is_selected)) {
                        *selected = index;
                        changed = true;
                    }

                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }

            return changed;
        });
    }

    static SDL_WindowID mouse_event_window_id(const SDL_Event& event) {
        switch (event.type) {
            case SDL_EVENT_MOUSE_MOTION:
                return event.motion.windowID;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                return event.button.windowID;
            default:
                return 0;
        }
    }

    static ImVec2 mouse_event_position(const SDL_Event& event) {
        if (event.type == SDL_EVENT_MOUSE_MOTION) {
            return {event.motion.x, event.motion.y};
        }

        return {event.button.x, event.button.y};
    }

    static bool draw_number_input(
        std::string_view label, ImGuiDataType type, void* values, int components, float speed, const void* minimum,
        const void* maximum, const char* format
    ) {
        if (components == 2) {
            return draw_labeled_input(label, [=] {
                const float total_width = std::min(INPUT_MAX_WIDTH, ImGui::GetContentRegionAvail().x);
                const float component_width = std::max(0.0F, (total_width - ITEM_SPACING - 20.0F) * 0.5F);
                const size_t value_size = type == ImGuiDataType_S32 ? sizeof(int) : sizeof(float);
                auto* raw_values = static_cast<unsigned char*>(values);
                bool changed = false;

                for (int index = 0; index < 2; ++index) {
                    if (index != 0) {
                        ImGui::SameLine(0.0F, ITEM_SPACING);
                    }

                    ImGui::PushID(index);
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextDisabled(index == 0 ? "x" : "y");
                    ImGui::SameLine(0.0F, 2.0F);
                    ImGui::SetNextItemWidth(component_width);
                    void* component = raw_values + value_size * static_cast<size_t>(index);
                    changed = (minimum != nullptr && maximum != nullptr
                                   ? ImGui::DragScalar("##component", type, component, speed, minimum, maximum, format)
                                   : ImGui::DragScalar("##component", type, component, speed, nullptr, nullptr, format)) ||
                              changed;
                    ImGui::PopID();
                }

                return changed;
            });
        }

        return draw_labeled_input(label, [=] {
            return minimum != nullptr && maximum != nullptr
                       ? ImGui::SliderScalarN("##value", type, values, components, minimum, maximum, format)
                       : ImGui::DragScalarN("##value", type, values, components, speed, minimum, maximum, format);
        });
    }

    static bool draw_number_input(
        std::string_view label, float* values, int components = 1, float speed = 0.1F, float minimum = 0.0F, float maximum = 0.0F
    ) {
        const bool limited = maximum > minimum;
        return draw_number_input(
            label, ImGuiDataType_Float, values, components, speed, limited ? &minimum : nullptr, limited ? &maximum : nullptr,
            "%.3f"
        );
    }

    static bool draw_number_input(
        std::string_view label, int* values, int components = 1, float speed = 1.0F, int minimum = 0, int maximum = 0
    ) {
        const bool limited = maximum > minimum;
        return draw_number_input(
            label, ImGuiDataType_S32, values, components, speed, limited ? &minimum : nullptr, limited ? &maximum : nullptr, "%d"
        );
    }

    Debugger::Debugger(UI& target) : m_target(target) {}

    Debugger::~Debugger() {
        shutdown();
    }

    void Debugger::shutdown() {
        if (m_target.profiler().enabled() && m_target.profiler().has_report()) {
            m_target.profiler().save_report();
        }

        if (m_ui == nullptr) {
            return;
        }

        m_ui.reset();

        m_target.backend().make_current();
    }

    void Debugger::setup() {
        ui::Config config{
            .title = "debugger",
            .size = {560.0F, 720.0F},
            .shared_with = &m_target.backend(),
            .resizable = true,
            .visible = false,
        };

        m_ui = std::make_unique<UI>(m_target.runtime(), config);

        if (!m_ui->ready()) {
            SDL_Log("Debugger: failed to initialize debugger UI");
            m_ui.reset();
            m_target.backend().make_current();
            return;
        }

        m_ui->backend().position_next_to(m_target.backend(), 16.0F);

        const ui::ImGuiContextScope scope(m_ui->imgui_context());

        m_icon.set_size(ICON_SIZE);
        m_icon.configure_all_styles([this](Style& style) { style.color(m_ui->theme().text_secondary_color); });

        m_target.backend().make_current();
    }

    void Debugger::set_icon(IconTexture* icon) {
        m_icon.set_texture(icon);
    }

    void Debugger::set_hotkey(ImGuiKeyChord hotkey) {
        m_hotkey = hotkey;
    }

    void Debugger::set_target(Node* target) {
        if (target != m_node_target) {
            m_highlight_selected = false;
        }

        m_node_target = target;
        m_profiling_target = target;
        m_target_identity = target == nullptr ? 0 : target->identity();
        m_target_was_flow_position = target != nullptr && !target->layout().has_explicit_position();
        m_select_properties = target != nullptr;

        auto* styled = dynamic_cast<StyledNode*>(target);
        m_inspected_style = styled == nullptr ? StyleType::DEFAULT : styled->style_type();

        if (m_node_target == nullptr) {
            m_highlight_valid = false;
            return;
        }

        refresh_highlight();
    }

    void Debugger::remove_target() {
        if (m_node_target == nullptr || m_node_target->parent() == nullptr) {
            return;
        }

        Node* target = m_node_target;
        Node* parent = target->parent();
        set_target(nullptr);
        m_hover_target = nullptr;
        m_scroll_to_target = false;

        if (std::unique_ptr<Node> detached = parent->remove(*target); detached != nullptr) {
            detached->set_visible(false);
            detached->set_enabled(false);
            m_detached_nodes.push_back(std::move(detached));
        }
    }

    bool Debugger::ready() const {
        return m_ui != nullptr && m_ui->ready();
    }

    void Debugger::set_enabled(bool enabled) {
        if (m_enabled == enabled) {
            return;
        }

        m_enabled = enabled;
        m_target.profiler().set_enabled(enabled);
        if (!enabled) {
            m_target.profiler().save_report();
        }

        if (m_ui == nullptr) {
            return;
        }

        if (m_enabled) {
            m_ui->backend().show();
            m_ui->backend().raise();
            return;
        }

        m_ui->backend().hide();
        set_inspect_mode(false);
    }

    void Debugger::set_style(const ImGuiStyle& style) {
        if (!ready()) {
            return;
        }

        const ui::ImGuiContextScope scope(m_ui->imgui_context());
        ImGui::GetStyle() = style;
    }

    void Debugger::set_font(FontType type, int size) {
        if (!ready()) {
            return;
        }

        const ui::ImGuiContextScope scope(m_ui->imgui_context());
        m_font = m_ui->get_font(type).get(size);

        if (m_font == nullptr) {
            SDL_Log("failed to load debugger font variation %d", size);
        }
    }

    bool Debugger::handle_inspect_event(const SDL_Event& event, bool mouse_event, SDL_WindowID main_window_id) {
        if (!m_inspect_mode || !mouse_event || mouse_event_window_id(event) != main_window_id) {
            return false;
        }

        Node* focused_node = m_target.input_router().debug_node_at(mouse_event_position(event));

        if (!focused_node) {
            m_hover_target = nullptr;
            return false;
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            set_target(focused_node);
            m_hover_target = nullptr;
            set_inspect_mode(false, true);
            m_scroll_to_target = true;
        } else {
            m_hover_target = focused_node;
        }

        return true;
    }

    bool Debugger::process_sdl_event(const SDL_Event* event) {
        if (event == nullptr || !ready()) {
            return false;
        }

        const SDL_WindowID debug_window_id = static_cast<SDL_WindowID>(m_ui->backend().window_id());
        const SDL_WindowID main_window_id = static_cast<SDL_WindowID>(m_target.backend().window_id());
        SDL_WindowID event_window_id = mouse_event_window_id(*event);
        const bool mouse_event = event_window_id != 0;

        if (event->type == SDL_EVENT_MOUSE_WHEEL) {
            event_window_id = event->wheel.windowID;
        }

        if (handle_inspect_event(*event, mouse_event, main_window_id)) {
            return true;
        }

        // inspect mode belongs to the main application window.
        // debugger controls will not receive mouse input.
        if (mouse_event && event_window_id != debug_window_id) {
            return false;
        }

        if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event->window.windowID == debug_window_id) {
            set_enabled(false);
            return true;
        }

        return ui::process_sdl_event(*m_ui, *event);
    }

    void Debugger::set_inspect_mode(bool enabled, bool wait_for_release) {
        m_inspect_mode = enabled;

        if (enabled) {
            m_target.input_router().set_debug_inspect_mode(true);
        } else if (wait_for_release) {
            m_target.input_router().finish_debug_inspect_mode();
        } else {
            m_target.input_router().clear_debug_inspect_mode();
        }

        if (!enabled) {
            m_hover_target = nullptr;
        }
    }

    void Debugger::update(float dt) {
        if (!ready()) {
            return;
        }

        m_icon.update(dt);

        const bool focused = m_target.backend().focused();

        if (focused && ImGui::IsKeyChordPressed(m_hotkey)) {
            set_enabled(!m_enabled);
        }

        m_target.input_router().set_debug_inspect_mode(m_inspect_mode);
    }

    void Debugger::refresh_highlight() {
        Node* target = m_inspect_mode ? m_hover_target : (m_highlight_selected ? m_node_target : nullptr);
        if (!m_target.root().contains(target) || !target->visible()) {
            m_highlight_valid = false;
            return;
        }

        const Rect rect = target->layout().screen_rect();
        if (rect.max.x <= rect.min.x || rect.max.y <= rect.min.y) {
            m_highlight_valid = false;
            return;
        }

        m_highlight = rect;
        m_highlight_valid = true;
    }

    bool Debugger::should_restore_flow_position() const {
        if (m_node_target == nullptr || !m_target_was_flow_position) {
            return false;
        }

        const NodeLayout& layout = m_node_target->layout();
        return layout.anchor() == Anchor::TopLeft && layout.origin() == Origin::TopLeft && layout.offset().x == 0.0F &&
               layout.offset().y == 0.0F;
    }

    void Debugger::draw_highlight() {
        refresh_highlight();

        if (!m_highlight_valid) {
            return;
        }

        ImGui::GetForegroundDrawList()->AddRect(
            m_highlight.min, m_highlight.max, ImColor(m_target.theme().accent_color), 0.0F, 0, 2.0F
        );
    }

    void Debugger::render_node_tree(Node& node, int depth, bool show_duration, Node*& selected_target, bool update_target) {
        ImGui::PushID(&node);
        ImGui::PushStyleColor(ImGuiCol_Text, node.visible() ? m_ui->theme().text_color : m_ui->theme().text_secondary_color);

        ImGuiTreeNodeFlags flags = depth < 1 ? ImGuiTreeNodeFlags_DefaultOpen : 0;
        flags |= ImGuiTreeNodeFlags_SpanAvailWidth;

        if (selected_target != nullptr && &node != selected_target && node.contains(selected_target)) {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        }

        if (&node == selected_target) {
            flags |= ImGuiTreeNodeFlags_Selected;
            ImGui::PushStyleColor(ImGuiCol_Header, m_ui->theme().accent_color);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, m_ui->theme().accent_hover_color);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, m_ui->theme().accent_color);
        }

        if (node.children().empty()) {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        } else {
            flags |= ImGuiTreeNodeFlags_OpenOnArrow;
        }

        std::string_view node_id = node.id();
        if (node_id.starts_with("##")) {
            node_id.remove_prefix(2);
        }

        std::string node_label{node.type_name()};
        if (!node_id.empty()) {
            node_label += " (";
            node_label += node_id;
            node_label += ")";
        }

        if (node_label.empty()) {
            node_label = node_id.empty() ? "Unknown" : std::string(node_id);
        }

        const double duration = m_target.profiler().node_duration_ms(node.identity());
        const bool expanded = show_duration ? ImGui::TreeNodeEx(&node, flags, "%s  %.3f ms", node_label.c_str(), duration)
                                            : ImGui::TreeNodeEx(&node, flags, "%s", node_label.c_str());
        const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

        if (&node == selected_target) {
            ImGui::PopStyleColor(3);
        }

        ImGui::PopStyleColor();
        ImGui::PopID();

        if (clicked) {
            const bool selected_again = selected_target == &node;
            selected_target = &node;
            if (update_target) {
                if (selected_again) {
                    m_highlight_selected = true;
                }
                set_target(&node);
                m_scroll_to_target = true;
            }
        }

        if (update_target && &node == selected_target && m_scroll_to_target) {
            const ImVec2 item_min = ImGui::GetItemRectMin();
            const ImVec2 item_max = ImGui::GetItemRectMax();
            if (!ImGui::IsRectVisible(item_min, item_max)) ImGui::SetScrollHereY(0.5F);
        }

        if (expanded) {
            for (const auto& child : node.children()) {
                render_node_tree(*child, depth + 1, show_duration, selected_target, update_target);
            }

            if (!node.children().empty()) {
                ImGui::TreePop();
            }
        }
    }

    void Debugger::render_node_properties() {
        draw_property_section("node");

        bool visible = m_node_target->visible();
        if (draw_labeled_input("visible", [&visible] { return ImGui::Checkbox("##value", &visible); })) {
            m_node_target->set_visible(visible);
        }

        bool enabled = m_node_target->enabled();
        if (draw_labeled_input("input enabled", [&enabled] { return ImGui::Checkbox("##value", &enabled); })) {
            m_node_target->set_enabled(enabled);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("controls input only; use visible to stop update and drawing");
        }

        draw_property_value("id", "%s", m_node_target->id().empty() ? "unnamed" : m_node_target->id().c_str());
        const std::string_view type = m_node_target->type_name();
        draw_property_value("type", "%.*s", static_cast<int>(type.size()), type.data());

        if (const std::optional<std::string> content = m_node_target->content(); content.has_value()) {
            std::string editable_content = *content;
            if (draw_text_input("content", editable_content)) {
                m_node_target->try_set_content(std::move(editable_content));
            }
        }

        draw_property_section("diagnostics");
        draw_property_value("identity", "%llu", static_cast<unsigned long long>(m_node_target->identity()));
        draw_property_value("children", "%zu", m_node_target->children().size());
        if (const auto* styled = dynamic_cast<const StyledNode*>(m_node_target); styled != nullptr) {
            draw_property_value("opacity", "%.3f", styled->opacity());
            draw_property_value("style state", "%s", STYLE_NAMES[static_cast<int>(styled->style_type())]);
        }

        draw_property_value("input layer", "%s", input_layer_name(m_node_target->input_layer()));
        draw_property_value("accepts input", "%s", m_node_target->accepts_input() ? "yes" : "no");
        draw_property_value("accepts focus", "%s", m_node_target->accepts_focus() ? "yes" : "no");
        draw_property_value("focused", "%s", m_target.input_router().focused_node() == m_node_target ? "yes" : "no");
    }

    void Debugger::render_profiling() {
        Profiler& profiler = m_target.profiler();
        if (ImGui::Button("save metrics")) {
            profiler.save_report();
        }
        ImGui::SameLine();
        if (ImGui::Button("clear metrics")) {
            profiler.clear_report();
        }

        for (const auto& child : m_target.root().children()) {
            render_node_tree(*child, 0, true, m_profiling_target, false);
        }
    }

    void Debugger::render_layout_properties() {
        draw_property_section("layout");

        const NodeLayout& layout = m_node_target->layout();
        draw_property_value("placement", "%s", layout.has_explicit_position() ? "explicit" : "flow");

        ImVec2 size = layout.size();
        if (draw_number_input("size", &size.x, 2)) {
            m_node_target->set_size(size);
        }

        ImVec2 offset = layout.offset();
        if (draw_number_input("offset", &offset.x, 2)) {
            m_node_target->set_offset(offset);
        }

        int anchor = static_cast<int>(layout.anchor());
        if (draw_inline_combo("anchor (parent)", &anchor, ALIGNMENT_NAMES, IM_ARRAYSIZE(ALIGNMENT_NAMES))) {
            m_node_target->set_anchor(static_cast<Anchor>(anchor));
            if (should_restore_flow_position()) {
                m_node_target->set_flow();
            }
        }

        int origin = static_cast<int>(layout.origin());
        if (draw_inline_combo("origin (node)", &origin, ALIGNMENT_NAMES, IM_ARRAYSIZE(ALIGNMENT_NAMES))) {
            m_node_target->set_origin(static_cast<Origin>(origin));
            if (should_restore_flow_position()) {
                m_node_target->set_flow();
            }
        }

        if (layout.anchor() == Anchor::Custom) {
            ImVec2 anchor_position = layout.anchor_factor();
            if (draw_number_input("anchor point", &anchor_position.x, 2, 0.01F)) {
                m_node_target->set_anchor_position(anchor_position);
            }
        }

        if (layout.origin() == Origin::Custom) {
            ImVec2 origin_position = layout.origin_factor();
            if (draw_number_input("origin point", &origin_position.x, 2, 0.01F)) {
                m_node_target->set_origin_position(origin_position);
            }
        }

        draw_property_section("resolved geometry");
        draw_rect_properties("arranged in parent", layout.arranged_rect());
        draw_rect_properties("screen bounds", layout.screen_rect());
        draw_rect_properties("parent content", layout.parent_content_rect());
    }

    void Debugger::render_style_variables(Style& style) {
        StyleVariableStore& variables = style.variables();

        m_variable_names.clear();
        variables.for_each([&](const std::string& name, const GenericValue&) {
            m_variable_names.push_back(name);
            return true;
        });
        std::sort(m_variable_names.begin(), m_variable_names.end());

        if (m_variable_names.empty()) {
            return;
        }

        draw_property_section("variables");
        for (const std::string& name : m_variable_names) {
            GenericValue* variable = variables.find(name);
            if (variable == nullptr) {
                continue;
            }

            std::visit(
                [&](auto& value) {
                    using ValueType = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<ValueType, FloatValue>) {
                        draw_number_input(name, &value.value, 1, 0.01F);
                    } else if constexpr (std::is_same_v<ValueType, IntValue>) {
                        draw_number_input(name, &value.value);
                    } else if constexpr (std::is_same_v<ValueType, BoolValue>) {
                        draw_labeled_input(name, [&value] { return ImGui::Checkbox("##value", &value.value); });
                    } else if constexpr (std::is_same_v<ValueType, StringValue>) {
                        draw_text_input(name, value.value);
                    } else if constexpr (std::is_same_v<ValueType, ColorValue>) {
                        draw_color_input(name, value.value.Value);
                    } else if constexpr (std::is_same_v<ValueType, Vec2Value>) {
                        draw_number_input(name, &value.value.x, 2, 0.01F);
                    }
                },
                *variable
            );
        }
    }

    void Debugger::render_style_properties() {
        auto* styled = dynamic_cast<StyledNode*>(m_node_target);

        if (styled == nullptr) {
            return;
        }

        draw_property_section("style");

        const bool app_focused = m_target.backend().focused();
        if (app_focused) {
            m_inspected_style = styled->style_type();
        }

        int style_index = static_cast<int>(m_inspected_style);
        if (draw_inline_combo("state", &style_index, STYLE_NAMES, IM_ARRAYSIZE(STYLE_NAMES))) {
            m_inspected_style = static_cast<StyleType>(style_index);
        }

        Style* style = &styled->style(m_inspected_style);

        bool supports_color = true;
        bool supports_background = true;
        bool supports_border = true;
        bool supports_padding = true;
        bool supports_radius = true;
        bool supports_thickness = true;
        if (styled->type_name() == "Text") {
            supports_background = false;
            supports_border = false;
            supports_padding = false;
            supports_radius = false;
            supports_thickness = false;
        } else if (styled->type_name() == "Line") {
            supports_background = false;
            supports_border = false;
            supports_padding = false;
            supports_radius = false;
        }

        if (supports_color) {
            ImVec4 color = style->color().get();
            if (draw_color_input("color", color)) {
                style->color().set(color);
            }
        }

        if (supports_background) {
            ImVec4 background_color = style->background_color().get();
            if (draw_color_input("background", background_color)) {
                style->background_color().set(background_color);
            }
        }

        if (supports_border) {
            ImVec4 border_color = style->border_color().get();
            if (draw_color_input("border", border_color)) {
                style->border_color().set(border_color);
            }
        }

        if (supports_padding) {
            ImVec2 padding = style->padding();
            if (draw_number_input("padding", &padding.x, 2, 0.1F, 0.0F, 128.0F)) {
                style->padding(padding);
            }
        }

        float alpha = style->alpha();
        if (draw_number_input("alpha", &alpha, 1, 0.01F, 0.0F, 1.0F)) {
            style->alpha(alpha);
        }

        if (supports_radius) {
            float radius = style->border_radius();
            if (draw_number_input("border radius", &radius, 1, 0.1F, 0.0F, 64.0F)) {
                style->border_radius(radius);
            }
        }

        if (supports_thickness) {
            float thickness = style->border_thickness();
            if (draw_number_input("border thickness", &thickness, 1, 0.1F, 0.0F, 16.0F)) {
                style->border_thickness(thickness);
            }
        }

        render_style_variables(*style);
    }

    void Debugger::render_properties() {
        if (m_node_target == nullptr) {
            ImGui::TextUnformatted("select a node from the list");
            return;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {ITEM_SPACING, 8.0F});
        render_node_properties();
        render_layout_properties();
        render_style_properties();
        ImGui::PopStyleVar();
        end_property_section();

        if (ImGui::Button("clear selection")) {
            set_target(nullptr);
            m_scroll_to_target = false;
        }

        ImGui::SameLine();
        const bool removable = m_node_target->parent() != nullptr;
        ImGui::BeginDisabled(!removable);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45F, 0.12F, 0.14F, 1.0F));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.58F, 0.16F, 0.18F, 1.0F));
        if (ImGui::Button("remove node")) {
            remove_target();
        }
        ImGui::PopStyleColor(2);
        ImGui::EndDisabled();
        if (!removable && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("the root node cannot be removed");
        }
    }

    void Debugger::render_toolbar() {
        const ImVec2 icon_position = ImGui::GetCursorScreenPos();
        const bool inspect_clicked = ImGui::InvisibleButton("##debug-inspect-mode", ICON_SIZE);

        ImGui::SetCursorScreenPos(icon_position);
        m_icon.style().color().set(ImColor(m_inspect_mode ? m_ui->theme().accent_color : m_ui->theme().text_secondary_color));
        m_icon.draw();

        if (inspect_clicked) {
            set_inspect_mode(!m_inspect_mode, m_inspect_mode);
        }

        ImGui::SameLine(0.0F, ITEM_SPACING);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (ICON_SIZE.y - ImGui::GetTextLineHeight()) * 0.5F);
        ImGui::TextUnformatted("inspect");

        const Profiler& profiler = m_target.profiler();
        ImGui::SameLine(0.0F, ITEM_SPACING * 2.0F);
        ImGui::TextDisabled(
            "%.2f ms render  %zu zones", profiler.latest_frame_ms(), static_cast<std::size_t>(profiler.latest_events().size())
        );
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("CPU frame work; present/VSync wait is excluded");
        }
        if (profiler.dropped_events() > 0) {
            ImGui::SameLine(0.0F, ITEM_SPACING);
            ImGui::TextColored(m_ui->theme().accent_color, "%u dropped", profiler.dropped_events());
        }

        ImGui::Separator();
    }

    void Debugger::render_node_list() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {WINDOW_PADDING, 4.0F});
        ImGui::BeginChild(
            "##debugger-nodes", {0.0F, 260.0F}, ImGuiChildFlags_Borders,
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar
        );
        ImGui::PopStyleVar();

        for (const auto& child : m_target.root().children()) {
            render_node_tree(*child, 0, false, m_node_target, true);
        }

        m_scroll_to_target = false;
        ImGui::EndChild();
    }

    void Debugger::render_sections() {
        ImGui::BeginChild(
            "##debugger-sections", {0.0F, 0.0F}, ImGuiChildFlags_Borders,
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar
        );

        if (ImGui::BeginTabBar("##debugger-sections-tabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
            const ImGuiTabItemFlags properties_flags =
                m_select_properties ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
            if (m_node_target != nullptr && ImGui::BeginTabItem("properties", nullptr, properties_flags)) {
                m_select_properties = false;
                ImGui::BeginChild(
                    "##debugger-properties-content", {0.0F, 0.0F}, ImGuiChildFlags_None,
                    ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar
                );
                render_properties();
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("profiling")) {
                ImGui::BeginChild(
                    "##debugger-profiling-content", {0.0F, 0.0F}, ImGuiChildFlags_None,
                    ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar
                );
                render_profiling();
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::EndChild();
    }

    void Debugger::render() {
        if (!m_enabled || !ready()) {
            return;
        }

        m_ui->begin_input_frame();
        m_ui->begin_frame();

        if (m_node_target != nullptr && !m_target.root().contains(m_node_target)) {
            set_target(nullptr);
            m_scroll_to_target = false;
        } else if (m_node_target != nullptr && m_node_target->identity() != m_target_identity) {
            set_target(nullptr);
            m_scroll_to_target = false;
        }

        if (m_profiling_target != nullptr && !m_target.root().contains(m_profiling_target)) {
            m_profiling_target = nullptr;
        }

        const ImVec2 display_size = m_ui->backend().display_size();

        ImGui::SetNextWindowPos({0.0F, 0.0F}, ImGuiCond_Always);
        ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {WINDOW_PADDING, WINDOW_PADDING});

        const bool debugger_visible = ImGui::Begin(
            "##ui-debugger", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings
        );
        if (debugger_visible) {
            const bool has_font = m_font != nullptr;

            if (has_font) {
                ImGui::PushFont(m_font);
            }

            ImGui::BeginChild(
                "##debugger-content", {0.0F, 0.0F}, ImGuiChildFlags_None,
                ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar
            );

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {ITEM_SPACING, 4.0F});
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {6.0F, 4.0F});
            render_toolbar();
            render_node_list();
            render_sections();
            ImGui::PopStyleVar(2);

            ImGui::EndChild();

            if (has_font) {
                ImGui::PopFont();
            }
        }

        ImGui::End();
        ImGui::PopStyleVar();

        m_ui->end_frame();
    }
} // namespace ui
