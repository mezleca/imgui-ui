#pragma once

#include "../widgets/widget.hpp"

namespace ui {
    /// a full-viewport container rendered in its own imgui window above normal content.
    class LayerContainer : public Widget {
    public:
        explicit LayerContainer(std::string id, std::string_view type_name = "LayerContainer");

    protected:
        void on_layout() override;
        bool paint() override;
        void on_draw_end() override;

    private:
        Rect m_rect{};
    };
} // namespace ui
