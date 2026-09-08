#pragma once

#include <imgui.h>

namespace ui {
    struct Theme {
        struct Controls {
            float rounding = 4.0F;
            float border_thickness = 1.0F;
            float thumb_size = 12.0F;

            ImVec4 background_color = {0.13F, 0.20F, 0.32F, 1.0F};
            ImVec4 hover_color = {0.22F, 0.39F, 0.62F, 1.0F};
            ImVec4 active_color = {0.26F, 0.59F, 0.98F, 0.67F};
            ImVec4 border_color = {0.35F, 0.42F, 0.58F, 1.0F};
            ImVec4 mark_color = {0.26F, 0.59F, 0.98F, 1.0F};
        } controls;

        struct Metrics {
            float window_rounding = 0.0F;
            float child_rounding = 0.0F;
            float popup_rounding = 6.0F;
            float tab_rounding = 4.0F;
            float frame_border_size = 0.0F;

            ImVec2 window_padding = {};
            ImVec2 cell_padding = {};
            ImVec2 frame_padding = {12.0F, 8.0F};
            ImVec2 item_spacing = {10.0F, 10.0F};
            ImVec2 item_inner_spacing = {8.0F, 6.0F};

            float circle_tessellation_max_error = 0.10F;
            bool anti_aliased_lines_use_tex = false;
        } metrics;

        struct Widgets {
            ImVec2 dropdown_item_padding = {10.0F, 4.0F};
            ImVec2 dropdown_arrow_size = {8.0F, 4.0F};
            float dropdown_popup_gap = 4.0F;
            float dropdown_transition_duration = 0.06F;

            float context_menu_width = 184.0F;
            float context_menu_item_height = 28.0F;
            ImVec2 context_menu_padding = {4.0F, 4.0F};
            ImVec2 context_menu_item_padding = {8.0F, 4.0F};
            float context_menu_gap = 6.0F;
            float context_menu_icon_size = 13.0F;

            ImVec2 text_input_padding = {12.0F, 14.0F};
            ImVec2 text_input_icon_size = {18.0F, 18.0F};
            float text_input_icon_spacing = 10.0F;
        } widgets;

        float content_padding = 12.0F;
        float box_rounding = 4.0F;
        float checkbox_rounding = 2.0F;

        ImVec4 accent_color = {0.26F, 0.59F, 0.98F, 1.0F};
        ImVec4 accent_hover_color = {0.42F, 0.70F, 1.0F, 1.0F};
        ImVec4 background_color = {0.06F, 0.065F, 0.085F, 0.94F};
        ImVec4 background_secondary_color = {0.10F, 0.11F, 0.14F, 1.0F};
        ImVec4 background_tertiary_color = {0.045F, 0.05F, 0.07F, 1.0F};
        ImVec4 scrollbar_background_color = {0.02F, 0.025F, 0.04F, 0.53F};
        ImVec4 header_background_color = {0.13F, 0.17F, 0.24F, 1.0F};
        ImVec4 text_color = {1.0F, 1.0F, 1.0F, 1.0F};
        ImVec4 text_secondary_color = {0.62F, 0.67F, 0.78F, 1.0F};
        ImVec4 border_color = {0.34F, 0.38F, 0.48F, 0.65F};
        ImVec4 header_border_color = {0.35F, 0.42F, 0.58F, 0.35F};
        ImVec4 button_active_color = {0.26F, 0.59F, 0.98F, 0.35F};
        ImVec4 transparent = {0.0F, 0.0F, 0.0F, 0.0F};
    };

} // namespace ui
