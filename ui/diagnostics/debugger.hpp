#pragma once

#include "../input/event.hpp"
#include "../layout/geometry.hpp"
#include "../style/style.hpp"

#include <imgui.h>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class UI;

namespace ui {
    class Node;
    class Style;
    class StyledNode;
    class Texture;

    class Debugger {
    public:
        ~Debugger();

        Debugger(const Debugger&) = delete;
        Debugger& operator=(const Debugger&) = delete;

        void set_enabled(bool enabled);
        void toggle() {
            set_enabled(!m_enabled);
        }

        bool enabled() const {
            return m_enabled;
        }

        /// handles overlay and inspect events before the application router.
        bool handle_input(UiEvent& event);
        /// toggles the hotkey and advances the idle timer.
        void update();
        /// renders the floating overlay after the application tree.
        void render();

        void set_style(const ImGuiStyle& style);
        void set_font(std::string_view id, int size);
        void set_hotkey(ImGuiKeyChord hotkey);

    private:
        friend class ::UI;

        explicit Debugger(UI& target);

        bool blocks_pointer_input() const {
            return m_inspect_mode || m_inspect_pointer_capture;
        }

        void render_toolbar();
        void render_node_list();
        void render_sections();
        void render_node_tree(Node& node, int depth);
        void render_properties();
        void render_node_properties();
        void render_profiling();
        void render_layout_properties();
        void render_style_properties();
        void render_style_controls(Style& style, bool is_line = false);
        void render_decoration_properties(StyledNode& node);
        void render_style_variables(Style& style);
        void draw_property_section(std::string_view label);
        void end_property_section();
        bool handle_inspect_event(UiEvent& event);
        void draw_highlight();
        void refresh_highlight();
        void synchronize_targets();
        void set_inspect_mode(bool enabled);
        void set_target(Node* target);
        void remove_target();
        bool should_restore_flow_position() const;
        static Node* pick_node(Node& root, ImVec2 position);
        bool overlay_contains(ImVec2 position) const;

        UI& m_target;
        ImFont* m_font = nullptr;
        Texture* m_inspect_icon = nullptr;
        Texture* m_close_icon = nullptr;
        Rect m_highlight{};
        bool m_highlight_valid = false;
        Rect m_overlay_rect{};
        Node* m_node_target = nullptr;
        Node* m_hover_target = nullptr;
        // removed nodes stay alive because application widgets may retain raw child pointers.
        std::vector<std::unique_ptr<Node>> m_detached_nodes;
        std::vector<std::string> m_variable_names;
        uint64_t m_target_identity = 0;
        uint64_t m_hover_identity = 0;
        StyleType m_inspected_style = StyleType::DEFAULT;
        ImGuiKeyChord m_hotkey = ImGuiMod_Shift | ImGuiKey_D;
        bool m_enabled = false;
        bool m_inspect_mode = false;
        bool m_target_was_flow_position = false;
        bool m_highlight_selected = false;
        bool m_select_properties = false;
        bool m_scroll_to_target = false;
        bool m_overlay_focused = false;
        float m_overlay_idle_time = 0.0F;
        bool m_overlay_pointer_capture = false;
        bool m_inspect_pointer_capture = false;
        bool m_property_section_open = false;
    };
} // namespace ui
