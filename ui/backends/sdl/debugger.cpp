#include "debugger.hpp"
#include "backend.hpp"
#include "../../ui.hpp"
#include "../opengl/texture-loader.hpp"
#include "../../resources/svg.hpp"
#include "../../style/styled-node.hpp"
#include "../../style/theme.hpp"
#include "../../imgui/context-scope.hpp"

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_log.h>
#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <format>
#include <string>
#include <utility>
#include <vector>

using namespace ui;
static Node* find_node_by_identity(Node& root, uint64_t identity) {
    if (root.identity() == identity) {
        return &root;
    }

    for (const auto& child : root.children()) {
        if (Node* result = find_node_by_identity(*child, identity); result != nullptr) {
            return result;
        }
    }

    return nullptr;
}

static bool contains_identity(const Node& root, uint64_t identity) {
    if (root.identity() == identity) {
        return true;
    }

    for (const auto& child : root.children()) {
        if (contains_identity(*child, identity)) {
            return true;
        }
    }

    return false;
}

static bool is_effectively_visible(const Node& node) {
    for (const Node* current = &node; current != nullptr; current = current->parent()) {
        if (!current->visible()) {
            return false;
        }

        if (const auto* styled = dynamic_cast<const StyledNode*>(current); styled != nullptr && !styled->visually_visible()) {
            return false;
        }
    }

    return true;
}

Node* Debugger::pick_node(Node& root, ImVec2 position) {
    if (!is_effectively_visible(root)) {
        return nullptr;
    }

    for (auto it = root.children().rbegin(); it != root.children().rend(); ++it) {
        if (Node* candidate = pick_node(**it, position); candidate != nullptr) {
            return candidate;
        }
    }

    if (!root.layout().visual_rect().contains(position)) {
        return nullptr;
    }

    if (dynamic_cast<const StyledNode*>(&root) == nullptr || !root.accepts_input()) {
        return nullptr;
    }

    return &root;
}

static constexpr const char* ALIGNMENT_NAMES[] = {
    "top-left",     "top-center",  "top-right",     "center-left",  "center",
    "center-right", "bottom-left", "bottom-center", "bottom-right", "custom",
};

static constexpr const char* STYLE_NAMES[] = {"default", "hover", "active", "focus"};
static constexpr const char* BORDER_STYLE_NAMES[] = {"solid", "dashed", "dotted"};

static std::string_view size_mode_name(LayoutSizeMode mode) {
    switch (mode) {
        case LayoutSizeMode::Fixed:
            return "fixed";
        case LayoutSizeMode::Fit:
            return "fit";
        case LayoutSizeMode::Grow:
            return "grow";
    }
    return "unknown";
}

static constexpr ImVec2 ICON_SIZE = {16.0F, 16.0F};
static constexpr float WINDOW_PADDING = 10.0F;
static constexpr float ITEM_SPACING = 12.0F;
static constexpr float INPUT_MAX_WIDTH = 180.0F;
static constexpr ImVec2 INPUT_PADDING = {0.0F, 0.0F};
static constexpr ImVec2 SECTION_PADDING = {10.0F, 8.0F};

static bool property_section_open = false;
static bool property_section_indented = false;

template <typename... Args>
static void draw_property_value(std::string_view label, std::string_view format, Args&&... args) {
    const std::string value = std::vformat(format, std::make_format_args(args...));
    ImGui::Text("%.*s:", static_cast<int>(label.size()), label.data());
    ImGui::SameLine(0.0F, ITEM_SPACING);
    ImGui::TextDisabled("%s", value.c_str());
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
        id.c_str(), {0.0F, 0.0F}, ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
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
    return draw_labeled_input(label, [&value] { return ImGui::ColorEdit4("##value", &value.x, ImGuiColorEditFlags_NoInputs); });
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

static bool draw_border_flags(std::string_view label, uint8_t* value) {
    return draw_labeled_input(label, [value] {
        bool changed = false;
        const auto draw_flag = [&](const char* name, uint8_t flag) {
            bool enabled = (*value & flag) != 0;
            if (ImGui::Checkbox(name, &enabled)) {
                *value = enabled ? static_cast<uint8_t>(*value | flag) : static_cast<uint8_t>(*value & ~flag);
                changed = true;
            }
        };

        draw_flag("left", BORDER_LEFT);
        ImGui::SameLine(0.0F, 4.0F);
        draw_flag("top", BORDER_TOP);
        ImGui::SameLine(0.0F, 4.0F);
        draw_flag("right", BORDER_RIGHT);
        ImGui::SameLine(0.0F, 4.0F);
        draw_flag("bottom", BORDER_BOTTOM);
        ImGui::SameLine(0.0F, 4.0F);

        bool all = (*value & BORDER_ALL) == BORDER_ALL;
        if (ImGui::Checkbox("all", &all)) {
            *value = all ? static_cast<uint8_t>(BORDER_ALL) : BORDER_NONE;
            changed = true;
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
        label, ImGuiDataType_Float, values, components, speed, limited ? &minimum : nullptr, limited ? &maximum : nullptr, "%.3f"
    );
}

static bool draw_slider(std::string_view label, float* value, float minimum, float maximum) {
    return draw_labeled_input(label, [=] {
        return ImGui::SliderFloat("##value", value, minimum, maximum, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    });
}

static bool
draw_number_input(std::string_view label, int* values, int components = 1, float speed = 1.0F, int minimum = 0, int maximum = 0) {
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

    if (m_inspect_icon != nullptr) {
        m_inspect_icon->release_context(m_ui->imgui_context());
        m_inspect_icon.reset();
    }

    m_ui.reset();

    m_target.backend().make_current();
}

void Debugger::setup() {
    BackendConfig config{
        .title = "debugger",
        .size = {560.0F, 720.0F},
        .shared_with = &m_target.backend(),
        .resizable = true,
        .visible = false,
        .swap_interval = 0,
    };

    m_ui = std::make_unique<UI>(m_target.runtime(), std::make_unique<SdlBackend>(std::move(config)));

    if (!m_ui->ready()) {
        SDL_Log("Debugger: failed to initialize debugger UI");
        m_ui.reset();
        m_target.backend().make_current();
        return;
    }

    m_ui->backend().position_next_to(m_target.backend(), 16.0F);

    const ImGuiContextScope scope(m_ui->imgui_context());

    OpenGLTextureLoader loader;
    m_inspect_icon = loader.load(INSPECT_SVG, "debugger-inspect");
    m_icon.set_texture(m_inspect_icon.get());
    m_icon.set_size({px(ICON_SIZE.x), px(ICON_SIZE.y)});
    m_icon.configure_all_styles([this](Style& style) { style.color(m_ui->theme().text_secondary_color); });

    m_target.backend().make_current();
}

void Debugger::set_icon(Texture* icon) {
    m_icon.set_texture(icon != nullptr ? icon : m_inspect_icon.get());
}

void Debugger::set_hotkey(ImGuiKeyChord hotkey) {
    m_hotkey = hotkey;
}

void Debugger::set_target(Node* target) {
    if (target != m_node_target) {
        m_highlight_selected = false;
    }

    m_node_target = target;
    m_target_identity = target == nullptr ? 0 : target->identity();
    m_target_was_flow_position = target != nullptr && target->layout().in_flow();
    m_select_properties = target != nullptr;

    auto* styled = dynamic_cast<StyledNode*>(target);
    m_inspected_style = styled == nullptr ? StyleType::DEFAULT : styled->style_type();

    if (m_node_target == nullptr) {
        m_highlight_valid = false;
        return;
    }

    refresh_highlight();
}

void Debugger::synchronize_targets() {
    Node* target = m_target_identity == 0 ? nullptr : find_node_by_identity(m_target.root(), m_target_identity);
    if (target != m_node_target) {
        set_target(target);
        if (target == nullptr) {
            m_scroll_to_target = false;
        }
    }

    if (m_hover_identity != 0) {
        m_hover_target = find_node_by_identity(m_target.root(), m_hover_identity);
        if (m_hover_target == nullptr) {
            m_hover_identity = 0;
        }
    } else {
        m_hover_target = nullptr;
    }
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
    if (!enabled) {
        m_target.profiler().set_enabled(false);
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

    const ImGuiContextScope scope(m_ui->imgui_context());
    ImGui::GetStyle() = style;
}

void Debugger::set_font(std::string_view id, int size) {
    if (!ready()) {
        return;
    }

    const ImGuiContextScope scope(m_ui->imgui_context());
    m_font = m_ui->get_font(id, size);

    if (m_font == nullptr) {
        SDL_Log("failed to load debugger font '%.*s' variation %d", static_cast<int>(id.size()), id.data(), size);
    }
}

bool Debugger::handle_inspect_event(const SDL_Event& event, bool mouse_event, SDL_WindowID main_window_id) {
    if (!m_inspect_mode || !mouse_event || mouse_event_window_id(event) != main_window_id) {
        return false;
    }

    Node* focused_node = pick_node(m_target.root(), mouse_event_position(event));

    if (!focused_node) {
        m_hover_target = nullptr;
        m_hover_identity = 0;
        return false;
    }

    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        set_target(focused_node);
        m_hover_target = nullptr;
        m_hover_identity = 0;
        set_inspect_mode(false, true);
        m_scroll_to_target = true;
    } else {
        m_hover_target = focused_node;
        m_hover_identity = focused_node->identity();
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

    if (mouse_event && event_window_id == main_window_id && event->type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
        event->button.button == SDL_BUTTON_LEFT && !m_inspect_mode && m_highlight_selected) {
        m_highlight_selected = false;
        m_highlight_valid = false;
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
        m_hover_identity = 0;
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
    const uint64_t target_identity = m_inspect_mode ? m_hover_identity : (m_highlight_selected ? m_target_identity : 0);
    Node* target = target_identity == 0 ? nullptr : find_node_by_identity(m_target.root(), target_identity);
    if (target == nullptr || !is_effectively_visible(*target)) {
        m_highlight_valid = false;
        return;
    }

    const Rect rect = target->layout().visual_rect();
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

    const Placement& placement = m_node_target->layout().placement();
    return placement.anchor == Anchor::TopLeft && placement.origin == Origin::TopLeft && placement.offset.x == 0.0F &&
           placement.offset.y == 0.0F;
}

void Debugger::draw_highlight() {
    synchronize_targets();
    refresh_highlight();

    if (!m_highlight_valid) {
        return;
    }

    ImGui::GetForegroundDrawList()->AddRect(
        m_highlight.min, m_highlight.max, ImColor(m_target.theme().accent_color), 0.0F, 0, 2.0F
    );
}

void Debugger::render_node_tree(Node& node, int depth, Node*& selected_target, bool update_target) {
    const bool effectively_visible = is_effectively_visible(node);
    ImGui::PushID(&node);
    ImGui::PushStyleColor(ImGuiCol_Text, effectively_visible ? m_ui->theme().text_color : m_ui->theme().text_secondary_color);

    ImGuiTreeNodeFlags flags = depth < 1 ? ImGuiTreeNodeFlags_DefaultOpen : 0;
    flags |= ImGuiTreeNodeFlags_SpanAvailWidth;

    if (m_target_identity != 0 && node.identity() != m_target_identity && contains_identity(node, m_target_identity)) {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }

    if (m_target_identity != 0 && node.identity() == m_target_identity) {
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

    const bool expanded = ImGui::TreeNodeEx(&node, flags, "%s", node_label.c_str());
    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

    if (m_target_identity != 0 && node.identity() == m_target_identity) {
        ImGui::PopStyleColor(3);
    }

    ImGui::PopStyleColor();
    ImGui::PopID();

    if (clicked) {
        if (update_target) {
            if (selected_target == &node) {
                m_highlight_selected = !m_highlight_selected;
            }
            set_target(&node);
            m_scroll_to_target = true;
        } else {
            selected_target = &node;
        }
    }

    if (update_target && &node == selected_target && m_scroll_to_target) {
        const ImVec2 item_min = ImGui::GetItemRectMin();
        const ImVec2 item_max = ImGui::GetItemRectMax();
        if (!ImGui::IsRectVisible(item_min, item_max)) ImGui::SetScrollHereY(0.5F);
    }

    if (expanded) {
        for (const auto& child : node.children()) {
            render_node_tree(*child, depth + 1, selected_target, update_target);
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

    draw_property_value("id", "{}", m_node_target->id().empty() ? "unnamed" : m_node_target->id().c_str());
    const std::string_view type = m_node_target->type_name();
    draw_property_value("type", "{}", type);
}

void Debugger::render_profiling() {
    const Profiler& profiler = m_target.profiler();
    if (!profiler.enabled()) {
        ImGui::TextDisabled("enable frame time to collect profiling data");
        return;
    }

    const ProfileFrameMetrics& metrics = profiler.latest_metrics();
    draw_property_section("frame");
    draw_property_value("frame", "{:.3f} ms", profiler.latest_frame_ms());
    draw_property_value("update", "{:.3f} ms", metrics.update_ms);
    draw_property_value("measure", "{:.3f} ms", metrics.measure_ms);
    draw_property_value("layout", "{:.3f} ms", metrics.layout_ms);
    draw_property_value("draw", "{:.3f} ms", metrics.draw_ms);
    draw_property_value("input", "{:.3f} ms", metrics.input_ms);
    draw_property_value("render", "{:.3f} ms", metrics.render_ms);
    end_property_section();

    draw_property_section("work");
    draw_property_value("nodes", "{}", metrics.nodes_drawn);
    draw_property_value("input work", "{} entries / {} checks", metrics.input_entries, metrics.input_entry_checks);
    draw_property_value("dropped events", "{}", profiler.dropped_events());
    end_property_section();

    if (m_node_target == nullptr) {
        return;
    }

    draw_property_section("selected node");
    draw_property_value("node total", "{:.3f} ms", profiler.node_duration_ms(m_node_target->identity()));

    for (const ProfileEvent& event : profiler.latest_events()) {
        if (event.node_identity != m_node_target->identity()) {
            continue;
        }

        draw_property_value(event.name, "{:.3f} ms", static_cast<double>(event.end - event.start) / 1'000'000.0);
    }
    end_property_section();
}

void Debugger::render_layout_properties() {
    draw_property_section("layout");

    const NodeLayout& layout = m_node_target->layout();
    const LayoutConfig& request = layout.config();
    const auto update_request = [&](auto&& update) {
        LayoutConfig config = request;
        update(config);
        m_node_target->set_layout(config);
    };
    draw_property_value("placement", "{}", request.in_flow ? "flow" : "explicit");

    ImVec2 size = layout.size();
    if (draw_number_input("size", &size.x, 2)) {
        m_node_target->set_size({px(size.x), px(size.y)});
    }

    const ImVec2 measured = layout.measured_size();
    const ImVec2 intrinsic = layout.intrinsic_size();
    const ImVec2 available = layout.available_size();
    const LayoutSize& size_spec = layout.size_spec();
    draw_property_value("size rule", "{} / {}", size_mode_name(size_spec.width.mode), size_mode_name(size_spec.height.mode));
    draw_property_value("measured", "{:.1f} x {:.1f}", measured.x, measured.y);
    draw_property_value("intrinsic", "{:.1f} x {:.1f}", intrinsic.x, intrinsic.y);
    draw_property_value("available", "{:.1f} x {:.1f}", available.x, available.y);

    const Rect arranged = layout.layout_rect();
    const Rect visual = layout.visual_rect();
    draw_property_value(
        "layout rect", "({:.1f}, {:.1f}) - ({:.1f}, {:.1f})", arranged.min.x, arranged.min.y, arranged.max.x, arranged.max.y
    );
    draw_property_value(
        "visual rect", "({:.1f}, {:.1f}) - ({:.1f}, {:.1f})", visual.min.x, visual.min.y, visual.max.x, visual.max.y
    );

    ImVec2 offset = request.placement.offset;
    if (draw_number_input("offset", &offset.x, 2)) {
        update_request([&](LayoutConfig& config) {
            config.placement.offset = offset;
            config.in_flow = false;
        });
    }

    int anchor = static_cast<int>(request.placement.anchor);
    if (draw_inline_combo("anchor (parent)", &anchor, ALIGNMENT_NAMES, IM_ARRAYSIZE(ALIGNMENT_NAMES))) {
        update_request([&](LayoutConfig& config) {
            config.placement.anchor = static_cast<Anchor>(anchor);
            config.in_flow = should_restore_flow_position();
        });
    }

    int origin = static_cast<int>(request.placement.origin);
    if (draw_inline_combo("origin (node)", &origin, ALIGNMENT_NAMES, IM_ARRAYSIZE(ALIGNMENT_NAMES))) {
        update_request([&](LayoutConfig& config) {
            config.placement.origin = static_cast<Origin>(origin);
            config.in_flow = should_restore_flow_position();
        });
    }

    if (request.placement.anchor == Anchor::Custom) {
        ImVec2 anchor_position = request.placement.anchor_position;
        if (draw_number_input("anchor point", &anchor_position.x, 2, 0.01F)) {
            update_request([&](LayoutConfig& config) {
                config.placement.anchor_position = anchor_position;
                config.in_flow = false;
            });
        }
    }

    if (request.placement.origin == Origin::Custom) {
        ImVec2 origin_position = request.placement.origin_position;
        if (draw_number_input("origin point", &origin_position.x, 2, 0.01F)) {
            update_request([&](LayoutConfig& config) {
                config.placement.origin_position = origin_position;
                config.in_flow = false;
            });
        }
    }
}

void Debugger::render_style_variables(Style& style) {
    StyleVariableStore& variables = style.variables();
    m_variable_names.clear();

    variables.for_each([&](const std::string& name, const StyleValue&) {
        m_variable_names.push_back(name);
        return true;
    });

    std::sort(m_variable_names.begin(), m_variable_names.end());

    if (m_variable_names.empty()) {
        return;
    }

    draw_property_section("variables");
    for (const std::string& name : m_variable_names) {
        StyleValue* variable = variables.find(name);
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

void Debugger::render_style_controls(Style& style, bool is_line) {
    ImVec4 color = style.color().get();
    if (draw_color_input("color", color)) {
        style.color().set(color);
    }

    if (!is_line) {
        ImVec4 background_color = style.background_color().get();
        if (draw_color_input("background", background_color)) {
            style.background_color().set(background_color);
        }

        ImVec4 border_color = style.border_color().get();
        if (draw_color_input("border color", border_color)) {
            style.border_color().set(border_color);
        }

        uint8_t border = style.border();
        if (draw_border_flags("border sides", &border)) {
            style.border(border);
        }

        int border_style = static_cast<int>(style.border_style());
        if (draw_inline_combo("border style", &border_style, BORDER_STYLE_NAMES, IM_ARRAYSIZE(BORDER_STYLE_NAMES))) {
            style.border_style(static_cast<BorderStyle>(border_style));
        }

        ImVec2 padding = style.padding();
        if (draw_number_input("padding", &padding.x, 2, 0.1F, 0.0F, 128.0F)) {
            style.padding(padding);
        }

        int blur = style.blur();
        if (draw_number_input("blur", &blur, 1, 1.0F, 0, 64)) {
            style.blur(blur);
        }

        BoxShadow shadow = style.box_shadow();
        if (draw_number_input("shadow offset", &shadow.offset.x, 2, 0.1F)) {
            style.box_shadow(shadow);
        }

        if (draw_slider("shadow blur", &shadow.blur, 0.0F, 256.0F)) {
            style.box_shadow(shadow);
        }

        if (draw_slider("shadow spread", &shadow.spread, -128.0F, 256.0F)) {
            style.box_shadow(shadow);
        }

        ImVec4 shadow_color = shadow.color.Value;
        if (draw_color_input("shadow color", shadow_color)) {
            shadow.color.Value = shadow_color;
            style.box_shadow(shadow);
        }

        float radius = style.border_radius();
        if (draw_number_input("border radius", &radius, 1, 0.1F, 0.0F, 64.0F)) {
            style.border_radius(radius);
        }
    }

    float alpha = style.alpha();
    if (draw_number_input("alpha", &alpha, 1, 0.01F, 0.0F, 1.0F)) {
        style.alpha(alpha);
    }

    float thickness = style.border_thickness();
    if (draw_number_input("border thickness", &thickness, 1, 0.1F, 0.0F, 16.0F)) {
        style.border_thickness(thickness);
    }
}

void Debugger::render_decoration_properties(StyledNode& node) {
    const auto render = [&](std::string_view label, bool before) {
        draw_property_section(label);

        bool enabled = before ? node.has_before() : node.has_after();
        if (draw_labeled_input("enabled", [&enabled] { return ImGui::Checkbox("##value", &enabled); })) {
            if (before) {
                if (enabled) {
                    node.before();
                } else {
                    node.remove_before();
                }
            } else {
                if (enabled) {
                    node.after();
                } else {
                    node.remove_after();
                }
            }
        }

        if (!enabled) {
            return;
        }

        PaintSlot& slot = before ? node.before() : node.after();
        render_style_controls(slot.style());
    };

    render("before", true);
    render("after", false);
}

void Debugger::render_style_properties() {
    auto* styled = dynamic_cast<StyledNode*>(m_node_target);
    if (styled == nullptr) {
        return;
    }

    draw_property_section("style");

    if (m_target.backend().focused()) {
        m_inspected_style = styled->style_type();
    }

    int style_index = static_cast<int>(m_inspected_style);
    if (draw_inline_combo("state", &style_index, STYLE_NAMES, IM_ARRAYSIZE(STYLE_NAMES))) {
        m_inspected_style = static_cast<StyleType>(style_index);
    }

    Style& style = styled->style(m_inspected_style);
    render_style_controls(style, styled->type_name() == "Line");
    render_style_variables(style);
    render_decoration_properties(*styled);
}

void Debugger::render_properties() {
    if (m_node_target == nullptr) {
        end_property_section();
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

    ImGui::Separator();
}

void Debugger::render_node_list() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {WINDOW_PADDING, 4.0F});
    ImGui::BeginChild(
        "##debugger-nodes", {0.0F, 340.0F}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar
    );
    ImGui::PopStyleVar();

    for (const auto& child : m_target.root().children()) {
        render_node_tree(*child, 0, m_node_target, true);
    }

    Profiler& profiler = m_target.profiler();

    const bool frame_time_enabled = profiler.enabled();
    const ImVec2 button_padding = {6.0F, 6.0F};
    const ImVec2 label_size = ImGui::CalcTextSize("frame time");
    const ImVec2 button_size = {label_size.x + button_padding.x * 2.0F, label_size.y + button_padding.y * 2.0F};
    const ImVec2 position = {
        ImGui::GetWindowPos().x + ImGui::GetWindowSize().x - button_size.x - 6.0F,
        ImGui::GetWindowPos().y + ImGui::GetWindowSize().y - button_size.y - 6.0F,
    };

    ImGui::SetCursorScreenPos(position);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});
    ImGui::BeginChild(
        "##debugger-frame-time", button_size, ImGuiChildFlags_None,
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    );
    ImGui::PopStyleVar();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, button_padding);

    if (frame_time_enabled) {
        ImGui::PushStyleColor(ImGuiCol_Button, m_ui->theme().accent_color);
    }

    if (ImGui::Button("frame time##toggle", button_size)) {
        profiler.set_enabled(!frame_time_enabled);
    }

    if (frame_time_enabled) {
        ImGui::PopStyleColor();
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();

    m_scroll_to_target = false;
    ImGui::EndChild();
}

void Debugger::render_sections() {
    ImGui::BeginChild(
        "##debugger-sections", {0.0F, 0.0F}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar
    );

    if (ImGui::BeginTabBar("##debugger-sections-tabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
        const ImGuiTabItemFlags properties_flags = m_select_properties ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
        if (ImGui::BeginTabItem("properties", nullptr, properties_flags)) {
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

    synchronize_targets();
    m_ui->begin_input_frame();
    m_ui->begin_frame();

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
            "##debugger-content", {0.0F, 0.0F}, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar
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
