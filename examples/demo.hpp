#pragma once

#include <ui/layout/stack-container.hpp>

#include <string>

class UI;

class DemoScreen final : public ui::StackContainer {
public:
    DemoScreen(UI& surface, std::string backend);

private:
    void on_update(float dt) override;

    UI& m_surface;
    bool m_enabled = true;
    int m_clicks = 0;
    std::string m_name = "imgui-ui";
    std::string m_theme = "blue";
    std::string m_applied_theme;
};

void setup_demo(UI& surface, std::string backend);
