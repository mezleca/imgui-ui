#pragma once

#include "container.hpp"

#include <string>

namespace ui {
    /// uses a native imgui tree header and reserves the remaining outer rect for its child body.
    class TreeContainer final : public Container {
    public:
        explicit TreeContainer(std::string label, std::string id = {});

    private:
        void on_measure() override;
        bool paint() override;
        void on_draw_end() override;
        ImVec2 child_window_size() const override;
        Rect shadow_rect(Rect child_rect) const override;
        ImVec2 child_layout_size() const override;

        std::string m_label;
        bool m_open = false;
        Rect m_outer_rect{};
        ImVec2 m_body_size{};
    };
} // namespace ui
