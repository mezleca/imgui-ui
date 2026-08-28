#include "modal-container.hpp"

#include "../ui.hpp"

namespace ui {
    static ModalPanel* active_modal(const Node& container) {
        for (auto it = container.children().rbegin(); it != container.children().rend(); ++it) {
            if ((*it)->visible()) {
                return static_cast<ModalPanel*>(it->get());
            }
        }

        return nullptr;
    }

    ModalContainer::ModalContainer(UI& ui)
        : LayerContainer("##modal-container", "ModalContainer"), m_ui(ui), m_input_router(ui.input_router()) {
        set_visible(true);
        set_input_layer(InputLayer::Modal);
        configure_all_styles([](Style& style) { style.background_color(ImColor{0.0F, 0.0F, 0.0F, 0.42F}); });

        _on_event = [this](UiEvent& event) {
            if (!has_open_modal()) {
                return;
            }

            const bool clicked_outside = (event.type == EventType::Click || event.type == EventType::PointerDown) &&
                                         event.button == PointerButton::Left &&
                                         (event.position.x < m_panel_min.x || event.position.x > m_panel_max.x ||
                                          event.position.y < m_panel_min.y || event.position.y > m_panel_max.y);
            const bool pressed_escape =
                event.type == EventType::Cancel || (event.type == EventType::KeyDown && event.key == Key::Escape);

            if (pressed_escape || clicked_outside) {
                close_top();
                event.stop_propagation();
            }
        };
    }

    ModalPanel& ModalContainer::open(std::string id) {
        if (ModalPanel* current = active(); current != nullptr) {
            current->set_visible(false);
        }

        ModalPanel& modal = add_child<ModalPanel>(m_ui, std::move(id));
        modal.set_visible(true);
        modal.fade_in();

        m_input_router.set_layer_policy(InputLayer::Modal, InputPolicy::BlockAll);
        m_input_router.set_keyboard_target(*this);
        return modal;
    }

    bool ModalContainer::close(ModalPanel& modal) {
        if (!contains(&modal) || m_pending_close == &modal) {
            return false;
        }

        // hide immediately so hit testing stops this frame, but defer ownership
        // removal until update to avoid mutating children during draw/event dispatch.
        const bool was_active = active() == &modal;
        modal.set_visible(false);
        m_pending_close = &modal;

        if (was_active) {
            if (ModalPanel* previous = active(); previous != nullptr) {
                previous->set_visible(true);
            } else {
                m_input_router.set_layer_policy(InputLayer::Modal, InputPolicy::PassThrough);
            }
        }

        return true;
    }

    void ModalContainer::close_top() {
        if (ModalPanel* current = active(); current != nullptr) {
            close(*current);
        }
    }

    ModalPanel* ModalContainer::active() {
        return active_modal(*this);
    }

    const ModalPanel* ModalContainer::active() const {
        return active_modal(*this);
    }

    bool ModalContainer::has_open_modal() const {
        return active() != nullptr;
    }

    void ModalContainer::on_update(float) {
        remove_pending_modal();

        if (has_open_modal()) {
            return;
        }

        // external visibility changes can leave no active panel without calling
        // close(), so input policy is reconciled every update.
        m_input_router.set_layer_policy(InputLayer::Modal, InputPolicy::PassThrough);
        m_input_router.clear_keyboard_target(InputLayer::Modal);
        m_input_router.clear_focus(*this);
        m_input_router.release_pointer(*this);
    }

    void ModalContainer::remove_pending_modal() {
        if (m_pending_close == nullptr) {
            return;
        }

        ModalPanel* modal = m_pending_close;
        m_pending_close = nullptr;

        m_input_router.clear_focus(*modal);
        m_input_router.clear_keyboard_target(*modal);
        remove(*modal);

        if (active() == nullptr) {
            m_input_router.set_layer_policy(InputLayer::Modal, InputPolicy::PassThrough);
            m_input_router.clear_keyboard_target(InputLayer::Modal);
        }
    }

    bool ModalContainer::on_draw() {
        if (!has_open_modal()) {
            return false;
        }

        return LayerContainer::on_draw();
    }

    void ModalContainer::draw_children() {
        if (ModalPanel* current = active(); current != nullptr) {
            current->draw();
            const Rect panel = current->layout().screen_rect();
            m_panel_min = panel.min;
            m_panel_max = panel.max;
        }
    }
} // namespace ui
