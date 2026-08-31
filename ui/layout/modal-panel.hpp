#pragma once

#include "stack-container.hpp"

class UI;

namespace ui {
    /// stacked modal content displayed inside ModalContainer and clamped to the viewport with an outer margin.
    class ModalPanel : public StackContainer {
    public:
        ModalPanel(UI& ui, std::string id);

        ModalPanel& set_margin(ImVec2 margin);
        const ImVec2& margin() const;

    protected:
        void on_layout() override;

    private:
        ImVec2 m_margin{};
    };
} // namespace ui
