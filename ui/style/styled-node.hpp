#pragma once

#include "state.hpp"
#include "../tree/node.hpp"

#include <imgui.h>
#include <string_view>
#include <utility>

namespace ui {
    class StyledNode : public Node {
    public:
        explicit StyledNode(std::string id = {}, std::string_view type_name = "StyledNode")
            : Node(std::move(id)), m_type_name(type_name) {
            m_state.set_change_callback(this, &StyledNode::style_changed);
        }
        StyledNode(const StyledNode&) = delete;
        StyledNode& operator=(const StyledNode&) = delete;

        std::string_view type_name() const override {
            return m_type_name;
        }

        /// effective values for the current transition.
        Style& style() {
            return m_state.style();
        }

        const Style& style() const {
            return m_state.style();
        }

        Style& style(StyleType type) {
            return m_state.style(type);
        }

        const Style& style(StyleType type) const {
            return m_state.style(type);
        }

        template <typename Func>
        StyledNode& configure_style(StyleType type, Func&& func) {
            m_state.configure_style(type, std::forward<Func>(func));
            return *this;
        }

        template <typename Func>
        StyledNode& configure_all_styles(Func&& func) {
            m_state.configure_all_styles(std::forward<Func>(func));
            return *this;
        }

        StyleType style_type() const {
            return m_state.style_type();
        }

        void set_visual_style(StyleType type) {
            m_state.set_style(type);
        }

        void set_interaction_style(bool hovered, bool active, bool focused = false) {
            m_state.set_item_state(hovered, active, focused);
        }

        void fade_in() {
            m_state.fade_in();
        }

        void fade_out() {
            m_state.fade_out();
        }

        void set_opacity(float opacity) {
            m_state.set_opacity(opacity);
        }

        float opacity() const {
            return m_state.opacity();
        }

        bool visually_visible() const {
            return m_state.is_visible();
        }

        bool accepts_visual_input() const {
            return m_state.accepts_input();
        }

        /// remeasures descendants because they may inherit this font.
        virtual StyledNode& set_font(ImFont* font) {
            ImFont* resolved_font = resolve_font(font);
            configure_all_styles([resolved_font](Style& style) { style.font(resolved_font); });
            invalidate_measure_subtree();
            return *this;
        }

        /// resolves the local font, then the closest styled ancestor, then imgui's font.
        ImFont* font() const {
            if (style().font() != nullptr) {
                return style().font();
            }

            for (const Node* ancestor = parent(); ancestor != nullptr; ancestor = ancestor->parent()) {
                const auto* styled_ancestor = dynamic_cast<const StyledNode*>(ancestor);
                if (styled_ancestor != nullptr && styled_ancestor->style().font() != nullptr) {
                    return styled_ancestor->style().font();
                }
            }

            return ImGui::GetCurrentContext() == nullptr ? nullptr : ImGui::GetFont();
        }

        void draw() override {
            if (!m_state.is_visible()) {
                return;
            }

            if (ImGui::GetCurrentContext() == nullptr) {
                Node::draw();
                return;
            }

            const Style& current_style = draw_style();
            const bool font_pushed = current_style.push(draw_opacity(), font());
            Node::draw();
            Style::pop(font_pushed);
        }

    protected:
        void set_type_name(std::string_view type_name) {
            m_type_name = type_name;
        }

        virtual const Style& draw_style() const {
            return style();
        }

        virtual float draw_opacity() const {
            return 1.0F;
        }

        void advance_frame_state(float dt) final {
            m_state.update(dt);
        }

    private:
        static void style_changed(void* owner) {
            static_cast<StyledNode*>(owner)->invalidate_measure();
        }

        static ImFont* resolve_font(ImFont* font) {
            return font != nullptr || ImGui::GetCurrentContext() == nullptr ? font : ImGui::GetFont();
        }

        VisualState m_state;
        std::string_view m_type_name;
    };
} // namespace ui
