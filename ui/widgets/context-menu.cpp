#include "context-menu.hpp"

#include "../style/theme.hpp"
#include "../ui.hpp"
#include "../imgui/draw.hpp"
#include "../resources/texture-registry.hpp"

#include <algorithm>
#include <utility>

using namespace ui;

static float menu_height(const Theme& theme, std::size_t item_count) {
    return theme.widgets.context_menu_item_height * static_cast<float>(item_count);
}

static Rect menu_work_area() {
    if (ImGui::GetCurrentContext() == nullptr) {
        return {};
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr) {
        return {};
    }

    const ImVec2 size =
        viewport->WorkSize.x > 0.0F && viewport->WorkSize.y > 0.0F ? viewport->WorkSize : ImGui::GetIO().DisplaySize;
    return Rect::from_position_size(viewport->WorkPos, size);
}

class ui::ContextMenuItemNode final : public DrawListWidget {
public:
    ContextMenuItemNode(ContextMenuWidget& menu, std::string label, ContextMenuCallback callback)
        : DrawListWidget("item", "ContextMenuItem"), m_menu(menu), m_label(std::move(label)), m_callback(std::move(callback)) {
        set_size({grow(), px(m_menu.m_theme.widgets.context_menu_item_height)});
        apply_theme_defaults(m_menu.m_theme);

        _on_event = [this](UiEvent& event) {
            if (event.type == EventType::PointerMove && m_submenu != nullptr) {
                m_menu.open_submenu(*this);
                return;
            }

            if (event.type != EventType::Click || event.button != PointerButton::Left) {
                return;
            }

            m_menu.activate_item(*this);
            event.stop_propagation();
        };
    }

    bool accepts_input() const override {
        return m_menu.is_open() && Widget::accepts_input();
    }

protected:
    void apply_theme_defaults(const Theme& theme) override {
        configure_all_styles([&theme](Style& style) {
            style.color(theme.text_color)
                .background_color(theme.transparent)
                .padding(theme.widgets.context_menu_item_padding)
                .border(BORDER_NONE)
                .border_radius(theme.controls.rounding)
                .cursor(ImGuiMouseCursor_Hand);
        });

        configure_style(StyleType::HOVER, [&theme](Style& style) { style.background_color(theme.controls.hover_color); });
        configure_style(StyleType::ACTIVE, [&theme](Style& style) { style.background_color(theme.controls.active_color); });
    }

private:
    friend class ContextMenuWidget;

    void paint_draw_list(ImDrawList& draw_list, Rect rect, const ComputedStyle& style) override {
        draw_frame(draw_list, rect, style);

        const ImVec2 padding = style.padding();
        const Rect content = rect.inset(padding);
        const ImVec2 text_size = ImGui::CalcTextSize(m_label.c_str());
        draw_text(
            draw_list, {content.min.x, content.min.y + (content.size().y - text_size.y) * 0.5F}, style.color().get_col(), m_label
        );

        if (m_submenu != nullptr) {
            draw_submenu_icon(draw_list, content, style);
        }
    }

    void draw_submenu_icon(ImDrawList& draw_list, Rect content, const ComputedStyle& style) const {
        const float icon_size =
            std::min(m_menu.m_theme.widgets.context_menu_icon_size, std::min(content.size().x, content.size().y));
        const Rect icon = Rect::from_position_size(
            {content.max.x - icon_size, content.min.y + (content.size().y - icon_size) * 0.5F}, {icon_size, icon_size}
        );

        if (m_submenu_icon == nullptr) {
            draw_triangle(
                draw_list, {icon.min.x + icon_size * 0.5F, icon.min.y + icon_size * 0.5F}, {icon_size * 0.5F, icon_size * 0.3F},
                style.color().get_col(), TriangleDirection::Right
            );
            return;
        }

        const ImTextureID texture = m_submenu_icon->get(icon.size());
        draw_list.AddImageQuad(
            texture, icon.min, {icon.max.x, icon.min.y}, icon.max, {icon.min.x, icon.max.y}, {0, 1}, {0, 0}, {1, 0}, {1, 1},
            style.color().get_col()
        );
    }

    ContextMenuWidget& m_menu;
    std::string m_label;
    ContextMenuCallback m_callback;
    ContextMenuWidget* m_submenu = nullptr;
    Texture* m_submenu_icon = nullptr;
};

ContextMenuWidget::ContextMenuWidget(UI& ui, ContextMenuItems items, Texture* submenu_icon)
    : ContextMenuWidget(
          ui.input_router(), ui.theme(),
          submenu_icon != nullptr ? submenu_icon : ui.runtime().textures().find("context-menu-chevron"), std::move(items)
      ) {}

ContextMenuWidget::ContextMenuWidget(InputRouter& router, const Theme& theme, Texture* submenu_icon, ContextMenuItems items)
    : StackContainer({}, StackDirection::Vertical), m_router(router), m_theme(theme), m_submenu_icon(submenu_icon) {
    set_type_name("ContextMenu");
    set_layout({.in_flow = false});
    set_visible(false);
    set_enabled(false);
    set_input_mode(InputMode::Target);

    _on_event = [this](UiEvent& event) {
        if (event.type == EventType::PointerMove) {
            root_menu().update_pointer_hover(event.position);
        }
    };

    apply_theme_defaults(theme);
    set_items(std::move(items));
}

void ContextMenuWidget::apply_theme_defaults(const Theme& theme) {
    set_size({px(theme.widgets.context_menu_width), px(menu_height(theme, m_items.size()))});

    configure_all_styles([&theme](Style& style) {
        style.padding({theme.widgets.context_menu_padding.x, 0.0F})
            .background_color(theme.background_secondary_color)
            .border(BORDER_ALL)
            .border_thickness(theme.controls.border_thickness)
            .border_radius(theme.box_rounding)
            .border_color(theme.border_color);
    });
}

ContextMenuWidget& ContextMenuWidget::set_items(ContextMenuItems items) {
    m_items.clear();
    clear();
    set_size({px(m_theme.widgets.context_menu_width), px(menu_height(m_theme, items.size()))});

    for (ContextMenuItem& item : items) {
        const bool has_submenu = !item.children.empty();
        auto& menu_item = add<ContextMenuItemNode>(*this, std::move(item.label), std::move(item.callback));
        m_items.push_back(&menu_item);

        if (!has_submenu) {
            continue;
        }

        auto submenu = std::unique_ptr<ContextMenuWidget>(
            new ContextMenuWidget(m_router, m_theme, m_submenu_icon, std::move(item.children))
        );
        submenu->m_parent_menu = this;
        menu_item.m_submenu = submenu.get();
        menu_item.m_submenu_icon = m_submenu_icon;
        attach(std::move(submenu));
    }

    return *this;
}

ContextMenuWidget& ContextMenuWidget::set_submenu_icon(Texture* icon) {
    if (m_submenu_icon == icon) {
        return *this;
    }

    m_submenu_icon = icon;

    for (ContextMenuItemNode* item : m_items) {
        if (item->m_submenu != nullptr) {
            item->m_submenu_icon = icon;
            item->m_submenu->set_submenu_icon(icon);
        }
    }

    return *this;
}

ContextMenuWidget& ContextMenuWidget::set_hover_close_delay(float seconds) {
    m_hover_close_delay_duration = std::max(0.0F, seconds);

    for (ContextMenuItemNode* item : m_items) {
        if (item->m_submenu != nullptr) {
            item->m_submenu->set_hover_close_delay(m_hover_close_delay_duration);
        }
    }

    return *this;
}

void ContextMenuWidget::open() {
    if (m_parent_menu != nullptr) {
        activate();
        return;
    }

    if (ImGui::GetCurrentContext() != nullptr) {
        open_at(ImGui::GetIO().MousePos);
    }
}

void ContextMenuWidget::open_at(ImVec2 screen_position) {
    if (m_parent_menu != nullptr) {
        open();
        return;
    }

    const Rect work_area = menu_work_area();
    if (!work_area.valid()) {
        return;
    }
    const ImVec2 position = clamp_position(work_area, layout().intrinsic_size(), screen_position);
    LayoutConfig config = layout().config();
    config.placement.offset = {position.x - work_area.min.x, position.y - work_area.min.y};
    set_layout(config);
    activate();
}

void ContextMenuWidget::activate() {
    if (m_open) {
        return;
    }

    m_closing = false;
    m_open = true;
    m_hover_close_delay = m_hover_close_delay_duration;
    if (m_parent_menu == nullptr) {
        m_pointer_was_inside = false;
    }
    set_visible(true);
    set_enabled(true);
    fade_in();
}

void ContextMenuWidget::close() {
    if (!m_open) {
        return;
    }

    m_open = false;
    m_closing = true;
    fade_out();
    close_children();
}

void ContextMenuWidget::cancel_close() {
    ContextMenuWidget& root = root_menu();
    if (root.m_closing) {
        root.activate();
    }
}

void ContextMenuWidget::on_update(float dt) {
    m_hover_close_delay = std::max(0.0F, m_hover_close_delay - dt);

    if (m_parent_menu == nullptr && m_open && ImGui::GetCurrentContext() != nullptr) {
        update_pointer_hover(ImGui::GetIO().MousePos);
    }

    if (m_closing && opacity() <= VISIBILITY_OPACITY_THRESHOLD) {
        m_closing = false;
        set_visible(false);
        set_enabled(false);
        set_opacity(0.0F);
    }
}

void ContextMenuWidget::draw_children() {
    for (ContextMenuItemNode* item : m_items) {
        item->draw();
    }
}

void ContextMenuWidget::on_draw_end() {
    StackContainer::on_draw_end();

    for (ContextMenuItemNode* item : m_items) {
        if (item->m_submenu == nullptr || !item->m_submenu->is_open()) {
            continue;
        }

        position_submenu(*item->m_submenu, *item);
    }

    if (m_parent_menu != nullptr || ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    const Rect work_area = menu_work_area();
    if (!work_area.valid()) {
        return;
    }
    m_router.register_blocker(*this, work_area, [this](UiEvent& event) {
        if (event.type == EventType::PointerMove) {
            update_pointer_hover(event.position);
            return;
        }

        if (event.type == EventType::PointerDown) {
            close();
        }
    });
}

void ContextMenuWidget::activate_item(ContextMenuItemNode& item) {
    if (item.m_submenu != nullptr) {
        open_submenu(item);
        return;
    }

    ContextMenuWidget& root = root_menu();
    root.close();
    if (item.m_callback) {
        item.m_callback(root);
    }
}

void ContextMenuWidget::open_submenu(ContextMenuItemNode& item) {
    for (ContextMenuItemNode* sibling : m_items) {
        if (sibling != &item && sibling->m_submenu != nullptr) {
            sibling->m_submenu->close();
        }
    }

    if (item.m_submenu != nullptr) {
        item.m_submenu->open();
    }
}

void ContextMenuWidget::update_pointer_hover(ImVec2 position) {
    if (contains_open_menu(position)) {
        m_pointer_was_inside = true;
        update_submenu_hover(position);
        return;
    }

    if (m_pointer_was_inside && m_hover_close_delay <= 0.0F) {
        close();
    }
}

void ContextMenuWidget::update_submenu_hover(ImVec2 position) {
    for (ContextMenuItemNode* item : m_items) {
        ContextMenuWidget* submenu = item->m_submenu;
        if (submenu == nullptr || !submenu->is_open()) {
            continue;
        }

        const Rect item_rect = item->layout().visual_rect();
        const Rect submenu_rect = submenu->layout().visual_rect();
        const Rect submenu_gap = {
            {std::min(item_rect.max.x, submenu_rect.max.x), std::max(item_rect.min.y, submenu_rect.min.y)},
            {std::max(item_rect.min.x, submenu_rect.min.x), std::min(item_rect.max.y, submenu_rect.max.y)},
        };

        if (submenu->m_hover_close_delay <= 0.0F && !item_rect.contains(position) && !submenu_gap.contains(position) &&
            !submenu->contains_open_menu(position)) {
            submenu->close();
        }
    }
}

void ContextMenuWidget::position_submenu(ContextMenuWidget& submenu, const ContextMenuItemNode& item) {
    const Rect item_rect = item.layout().visual_rect();
    const Rect work_area = menu_work_area();
    const ImVec2 submenu_size = submenu.layout().intrinsic_size();

    const float submenu_gap = m_theme.widgets.context_menu_gap;
    float screen_x = item_rect.max.x + submenu_gap;
    if (screen_x + submenu_size.x > work_area.max.x) {
        screen_x = item_rect.min.x - submenu_size.x - submenu_gap;
    }

    const ImVec2 position = clamp_position(work_area, submenu_size, {screen_x, item_rect.min.y});
    const ImVec2 window_position = ImGui::GetWindowPos();
    const ImVec2 content_offset = ImGui::GetCursorStartPos();
    const ImVec2 content_origin = {window_position.x + content_offset.x, window_position.y + content_offset.y};
    LayoutConfig config = submenu.layout().config();
    config.placement.offset = {position.x - content_origin.x, position.y - content_origin.y};
    config.in_flow = false;
    submenu.set_layout(config);
    submenu.draw();
}

bool ContextMenuWidget::contains_open_menu(ImVec2 position) const {
    if (layout().visual_rect().contains(position)) {
        return true;
    }

    for (const ContextMenuItemNode* item : m_items) {
        if (item->m_submenu != nullptr && item->m_submenu->is_open() && item->m_submenu->contains_open_menu(position)) {
            return true;
        }
    }

    return false;
}

ContextMenuWidget& ContextMenuWidget::root_menu() {
    ContextMenuWidget* root = this;
    while (root->m_parent_menu != nullptr) {
        root = root->m_parent_menu;
    }

    return *root;
}

void ContextMenuWidget::close_children() {
    for (ContextMenuItemNode* item : m_items) {
        if (item->m_submenu != nullptr) {
            item->m_submenu->close();
        }
    }
}
