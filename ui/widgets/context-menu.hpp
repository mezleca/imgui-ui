#pragma once

#include "../layout/stack-container.hpp"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace ui {
    class UI;
    class Texture;
    class ContextMenuItemNode;
    class ContextMenuWidget;

    using ContextMenuCallback = std::function<void(ContextMenuWidget&)>;

    struct ContextMenuItem {
        std::string label;
        std::vector<ContextMenuItem> children;
        ContextMenuCallback callback;

        static ContextMenuItem action(std::string label, ContextMenuCallback callback = {}) {
            return {.label = std::move(label), .children = {}, .callback = std::move(callback)};
        }

        static ContextMenuItem submenu(std::string label, std::vector<ContextMenuItem> children) {
            return {.label = std::move(label), .children = std::move(children), .callback = {}};
        }
    };

    using ContextMenuItems = std::vector<ContextMenuItem>;

    class ContextMenuWidget : public StackContainer {
    public:
        ContextMenuWidget(UI& ui, ContextMenuItems items = {}, Texture* submenu_icon = nullptr);

        ContextMenuWidget& set_items(ContextMenuItems items);
        ContextMenuWidget& set_submenu_icon(Texture* icon);

        /// waits this many seconds after opening before hover can close this menu or its submenus.
        ContextMenuWidget& set_hover_close_delay(float seconds);

        void open();
        void open_at(ImVec2 screen_position);
        void close();
        void cancel_close();

        bool is_open() const {
            return m_open;
        }

        bool accepts_input() const override {
            return m_open && Widget::accepts_input();
        }

    protected:
        void apply_theme_defaults(const Theme& theme) override;

    private:
        friend class ContextMenuItemNode;

        ContextMenuWidget(InputRouter& router, const Theme& theme, Texture* submenu_icon, ContextMenuItems items);

        void on_update(float) override;
        void on_event(UiEvent& event) override;
        void draw_children() override;
        void on_draw_end() override;

        void activate_item(ContextMenuItemNode& item);
        void open_submenu(ContextMenuItemNode& item);
        void update_pointer_hover(ImVec2 position);
        void update_submenu_hover(ImVec2 position);
        void activate();
        void position_submenu(ContextMenuWidget& submenu, const ContextMenuItemNode& item);
        bool contains_open_menu(ImVec2 position) const;
        ContextMenuWidget& root_menu();
        void close_children();

        InputRouter& m_router;
        const Theme& m_theme;
        Texture* m_submenu_icon = nullptr;
        std::vector<ContextMenuItemNode*> m_items;
        ContextMenuWidget* m_parent_menu = nullptr;
        bool m_open = false;
        bool m_closing = false;
        bool m_pointer_was_inside = false;
        float m_hover_close_delay_duration = 1.0F;
        float m_hover_close_delay = 0.0F;
    };
} // namespace ui
