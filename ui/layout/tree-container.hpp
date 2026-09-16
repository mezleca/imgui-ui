#pragma once

#include "container.hpp"

#include <string>

namespace ui {
    /// lays out children below a collapsible native imgui tree header.
    class TreeContainer final : public Container {
    public:
        explicit TreeContainer(std::string label, std::string id = {});

        [[nodiscard]] bool open() const {
            return m_open;
        }

    private:
        void on_measure() override;
        bool paint() override;
        void draw_children() override;
        void on_draw_end() override;

        std::string m_label;
        bool m_open = false;
    };
} // namespace ui
