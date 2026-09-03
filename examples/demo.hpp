#pragma once

#include <ui/layout/resizable-container.hpp>
#include <ui/layout/stack-container.hpp>
#include <ui/widgets/text.hpp>

#include <string>
#include <vector>

class UI;

class DemoTextListWidget final : public ui::StackContainer {
public:
    explicit DemoTextListWidget(std::vector<std::string> items);
    DemoTextListWidget& set_items(std::vector<std::string> items);
};

class DemoScreen final : public ui::StackContainer {
public:
    DemoScreen(UI& surface, std::string backend);
    void setup_dynamic_nodes(ui::Node& parent);
    int& blur();

private:
    void on_update(float dt) override;
    static void apply_border_style(ui::Node& node, ui::BorderStyle style);

    UI& m_surface;
    ui::ResizableContainer* m_dynamic_nodes = nullptr;
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
    std::string m_border_style = "solid";
    int m_blur = 5;
};

void setup_demo(UI& surface, std::string backend);
