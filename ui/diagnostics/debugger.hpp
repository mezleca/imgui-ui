#pragma once

#include "../input/event.hpp"
#include "../layout/container.hpp"
#include "../style/style.hpp"

#include <imgui.h>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ui {
    class UI;
    class Node;
    class Style;
    class StyledNode;
    class Texture;
    class DebuggerPopupState;

    class Debugger final : public Container {
    public:
        explicit Debugger(UI& target);
        ~Debugger();

        Debugger(const Debugger&) = delete;
        Debugger& operator=(const Debugger&) = delete;

        void set_open(bool open);
        void toggle() {
            set_open(!m_open);
        }

        bool is_open() const {
            return m_open;
        }

        /// handles overlay and inspect events before the application router.
        bool handle_input(UiEvent& event);
        void handle_hotkey();
        /// renders the diagnostic panel in the surface layout.
        void render();

        void set_style(const ImGuiStyle& style);
        void set_font(std::string_view id, int size);
        void set_hotkey(ImGuiKeyChord hotkey);

    private:
        friend class UI;

        bool blocks_pointer_input() const {
            return m_inspect_mode || m_inspect_pointer_capture;
        }

        void render_toolbar();
        void render_node_list(float height);
        void render_sections();
        void render_node_tree(Node& node, int depth);
        void render_properties();
        void render_node_properties();
        void render_profiling();
        void render_layout_properties();
        void render_style_properties();
        void render_style_controls(Style& style, bool is_line = false, std::span<Style*> all_styles = {});
        void render_decoration_properties(StyledNode& node);
        void render_style_variables(Style& style, std::span<Style*> all_styles = {});
        void draw_property_section(std::string_view label);
        void end_property_section();
        bool handle_inspect_event(UiEvent& event);
        bool handles_content_resize(const UiEvent& event) const;
        void draw_highlight();
        void refresh_highlight();
        void synchronize_targets();
        void set_inspect_mode(bool enabled);
        void finish_popup_restore();
        void set_target(Node* target);
        void remove_target();
        bool should_restore_flow_position() const;
        bool overlay_contains(ImVec2 position) const;

        void draw_children() override;
        void apply_theme_defaults(const Theme& theme) override;

        UI& m_target;
        ImFont* m_font = nullptr;
        Texture* m_inspect_icon = nullptr;
        Texture* m_close_icon = nullptr;
        Rect m_highlight{};
        bool m_highlight_valid = false;
        Rect m_overlay_rect{};
        ImGuiID m_overlay_window_id = 0;
        Node* m_node_target = nullptr;
        Node* m_hover_target = nullptr;
        // removed nodes stay alive because application widgets may retain raw child pointers.
        std::vector<std::unique_ptr<Node>> m_detached_nodes;
        std::vector<std::string> m_variable_names;
        uint64_t m_target_identity = 0;
        uint64_t m_hover_identity = 0;
        int m_inspected_style = 0;
        float m_node_list_ratio = 0.6F;
        ImGuiKeyChord m_hotkey = ImGuiMod_Shift | ImGuiKey_D;
        bool m_open = false;
        bool m_inspect_mode = false;
        bool m_target_was_flow_position = false;
        bool m_highlight_selected = false;
        bool m_select_properties = false;
        bool m_scroll_to_target = false;
        bool m_overlay_focused = false;
        bool m_overlay_pointer_capture = false;
        bool m_inspect_pointer_capture = false;
        bool m_property_section_open = false;
        std::unique_ptr<DebuggerPopupState> m_popup_state;
    };
} // namespace ui
