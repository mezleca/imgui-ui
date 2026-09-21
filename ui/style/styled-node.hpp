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
    /// extended Node with style slots, animated computed values, and custom paint hooks.
    class StyledNode : public Node {
    public:
        explicit StyledNode(std::string id = {}, std::string_view type_name = "StyledNode");
        ~StyledNode() override;
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

        const ComputedStyle& computed_style() const {
            return m_state.computed_style();
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

        StyleAnimationSequence animate() {
            return m_state.animate();
        }

        Animator& animator() {
            return m_state.animator();
        }

        void cancel_animations() {
            m_state.cancel_animations();
        }

        void fade_in() {
            m_state.fade_in();
        }

        void fade_in(TransitionSpec transition) {
            m_state.fade_in(transition);
        }

        void fade_out() {
            m_state.fade_out();
        }

        void fade_out(TransitionSpec transition) {
            m_state.fade_out(transition);
        }

        void set_opacity(float opacity) {
            m_state.set_opacity(opacity);
        }

        void set_opacity(float opacity, TransitionSpec transition) {
            m_state.set_opacity(opacity, transition);
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

        ImVec2 layout_margin() const override {
            return computed_style().margin();
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
            configure_all_styles([font](Style& style) { style.font(font); });
            invalidate_measure_subtree();
            return *this;
        }

        /// resolves the local font, then the closest styled ancestor, then imgui's font.
        ImFont* font() const {
            const ComputedStyle& current_style = computed_style();
            if (current_style.font() != nullptr) {
                return current_style.font();
            }

            if (m_font_cache_valid) {
                return m_cached_font;
            }

            for (const Node* ancestor = parent(); ancestor != nullptr; ancestor = ancestor->parent()) {
                const auto* styled_ancestor = dynamic_cast<const StyledNode*>(ancestor);
                if (styled_ancestor == nullptr) {
                    continue;
                }

                const ComputedStyle& ancestor_style = styled_ancestor->computed_style();
                if (ancestor_style.font() != nullptr) {
                    m_cached_font = ancestor_style.font();
                    m_font_cache_valid = true;
                    return m_cached_font;
                }
            }

            return ImGui::GetFont();
        }

        void draw() override;

    protected:
        bool on_draw() final;
        /// paints this node and returns whether its children should be drawn.
        virtual bool paint();

        void set_type_name(std::string_view type_name) {
            m_type_name = type_name;
        }

        void advance_frame_state(float dt) final;
        void input_state_changed() override;
        void draw_before() override;
        void draw_after() override;

        BoxInsets box_insets() const override {
            const ComputedStyle& style = computed_style();
            const ImVec2 padding = style.padding();
            const float thickness = style.border_thickness();
            const uint8_t border = style.border();
            return {
                padding.x + ((border & BORDER_LEFT) != 0 ? thickness : 0.0F),
                padding.y + ((border & BORDER_TOP) != 0 ? thickness : 0.0F),
                padding.x + ((border & BORDER_RIGHT) != 0 ? thickness : 0.0F),
                padding.y + ((border & BORDER_BOTTOM) != 0 ? thickness : 0.0F),
            };
        }

        BoxSizing box_sizing() const override {
            return computed_style().box_sizing();
        }

        float minimum_content_height() const override {
            ImGui::PushFont(font());
            const float line_height = ImGui::GetTextLineHeight();
            ImGui::PopFont();
            return line_height * computed_style().line_height();
        }

        void draw_surface(ImDrawList& draw_list, Rect rect) const;
        void draw_surface(ImDrawList& draw_list, Rect rect, ImColor background) const;

    private:
        static void style_changed(void* owner);

        void invalidate_font_cache_subtree() {
            m_font_cache_valid = false;
            for (const auto& child : children()) {
                if (child->removal_pending()) {
                    continue;
                }

                if (auto* styled_child = dynamic_cast<StyledNode*>(child.get()); styled_child != nullptr) {
                    styled_child->invalidate_font_cache_subtree();
                }
            }
        }

        void update_cursor();

        VisualState m_state;
        std::string_view m_type_name;
        std::unique_ptr<PaintSlot> m_before;
        std::unique_ptr<PaintSlot> m_after;
        mutable ImFont* m_cached_font = nullptr;
        mutable bool m_font_cache_valid = false;
    };
} // namespace ui
