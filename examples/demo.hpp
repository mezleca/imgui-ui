#pragma once

#include <ui/layout/stack-container.hpp>
#include <ui/widgets/text.hpp>

#include <string>
#include <vector>

class UI;

class DemoTextListWidget final : public ui::StackContainer {
public:
    explicit DemoTextListWidget(std::vector<std::string> items);

    /// replaces the rendered text items.
    DemoTextListWidget& set_items(std::vector<std::string> items);
};

class DemoScreen final : public ui::StackContainer {
public:
    DemoScreen(UI& surface, std::string backend);
    void setup_dynamic_nodes(ui::Node& parent);

private:
    void on_update(float dt) override;

    UI& m_surface;
    ui::StackContainer* m_dynamic_nodes = nullptr;
    ui::TextWidget* m_dynamic_status = nullptr;
    ui::Node* m_pending_remove = nullptr;
    bool m_enabled = true;
    int m_clicks = 0;
    DemoTextListWidget* m_text_list = nullptr;
    bool m_text_list_horizontal = false;
    int m_dynamic_count = 0;
    int m_next_dynamic_id = 0;
    std::string m_name = "imgui-ui";
    std::string m_theme = "blue";
    std::string m_applied_theme;
};

void setup_demo(UI& surface, std::string backend);
