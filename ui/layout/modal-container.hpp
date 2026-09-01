#pragma once

#include "../input/router.hpp"
#include "layer-container.hpp"
#include "modal-panel.hpp"

#include <utility>

class UI;

namespace ui {
    /// owns the full-screen modal layer; its visible ModalPanel is the stacked content window that receives modal input.
    class ModalContainer final : public LayerContainer {
    public:
        explicit ModalContainer(UI& ui);

        ModalPanel& open(std::string id = "modal");

        template <typename T, typename... Args>
        T& open(std::string id, Args&&... args) {
            ModalPanel& modal = open(std::move(id));
            return modal.add_child<T>(std::forward<Args>(args)...);
        }

        /// schedules removal after the current draw lifecycle.
        bool close(ModalPanel& modal);
        void close_top();

        ModalPanel* active();
        const ModalPanel* active() const;
        bool has_open_modal() const;

        bool debug_selectable() const override {
            return has_open_modal();
        }

    protected:
        void on_update(float dt) override;
        bool paint() override;
        void draw_children() override;

    private:
        void handle_event(UiEvent& event);
        void remove_pending_modal();

        UI& m_ui;
        InputRouter& m_input_router;
        ModalPanel* m_pending_close = nullptr;
        ImVec2 m_panel_min{};
        ImVec2 m_panel_max{};
    };
} // namespace ui
