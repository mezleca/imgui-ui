#pragma once

#include "paint-slot.hpp"
#include "state.hpp"
#include "../tree/node.hpp"

#include <imgui.h>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace ui {
    class StyledNode : public Node {
    public:
        explicit StyledNode(std::string id = {}, std::string_view type_name = "StyledNode");
        ~StyledNode() override;
        StyledNode(const StyledNode&) = delete;
        StyledNode& operator=(const StyledNode&) = delete;

        std::string_view type_name() const override {
            return m_type_name;
        }

        bool debug_selectable() const override {
            return true;
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

        /// creates the paint slot rendered before this node's contents on first access.
        PaintSlot& before();

        bool has_before() const {
            return m_before != nullptr;
        }

        /// creates the paint slot rendered after this node's contents on first access.
        PaintSlot& after();

        bool has_after() const {
            return m_after != nullptr;
        }

        void remove_before();
        void remove_after();

        /// remeasures descendants because they may inherit this font.
        StyledNode& set_font(ImFont* font) {
            ImFont* resolved_font = resolve_font(font);
            configure_all_styles([resolved_font](Style& style) { style.font(resolved_font); });
            invalidate_measure_subtree();
            return *this;
        }

        /// resolves the local font, then the closest styled ancestor, then imgui's font.
        ImFont* font() const {
            const Style& current_style = style();
            if (current_style.font() != nullptr) {
                return current_style.font();
            }

            for (const Node* ancestor = parent(); ancestor != nullptr; ancestor = ancestor->parent()) {
                const auto* styled_ancestor = dynamic_cast<const StyledNode*>(ancestor);
                if (styled_ancestor == nullptr) {
                    continue;
                }

                const Style& ancestor_style = styled_ancestor->style();
                if (ancestor_style.font() != nullptr) {
                    return ancestor_style.font();
                }
            }

            return ImGui::GetCurrentContext() == nullptr ? nullptr : ImGui::GetFont();
        }

        void draw() override;

    protected:
        bool on_draw() final;
        /// draws this styled node before Node draws its children.
        virtual bool paint_content();

        void set_type_name(std::string_view type_name) {
            m_type_name = type_name;
        }

        void advance_frame_state(float dt) final;
        void input_state_changed() override;
        void draw_before() override;
        void draw_after() override;

    private:
        void update_cursor();

        static void style_changed(void* owner) {
            static_cast<StyledNode*>(owner)->invalidate_measure();
        }

        static ImFont* resolve_font(ImFont* font) {
            return font != nullptr || ImGui::GetCurrentContext() == nullptr ? font : ImGui::GetFont();
        }

        VisualState m_state;
        std::string_view m_type_name;
        std::unique_ptr<PaintSlot> m_before;
        std::unique_ptr<PaintSlot> m_after;
        bool m_cursor_applied = false;
    };
} // namespace ui
