#pragma once

#include "paint-slot.hpp"
#include "state.hpp"
#include "../tree/node.hpp"

#include <imgui.h>
#include <memory>
#include <optional>
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

        /// animates this node's style properties, including scale and rotation.
        StyleAnimationSequence animate() {
            return m_state.animate();
        }

        /// returns the timeline updated while this node is visible. its callbacks stop advancing when the node is hidden.
        Animator& animator() {
            return m_state.animator();
        }

        void cancel_animations() {
            m_state.cancel_animations();
        }

        void fade_in(TransitionSpec transition = {OPACITY_TRANSITION_DURATION, easing::linear}) {
            m_state.fade_in(transition);
        }

        /// animates opacity to zero. the node remains visible until set_visible(false) is called.
        void fade_out(TransitionSpec transition = {OPACITY_TRANSITION_DURATION, easing::linear}) {
            m_state.fade_out(transition);
        }

        void set_opacity(float opacity, TransitionSpec transition = {OPACITY_TRANSITION_DURATION, easing::linear}) {
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
        StyledNode& set_font(ImFont* font);

        /// resolves the local font, then the closest styled ancestor, then imgui's font.
        ImFont* font() const;

        void draw() override;

    protected:
        void set_surface(Surface* surface) override;
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

        BoxInsets box_insets() const override;

        BoxSizing box_sizing() const override {
            return computed_style().box_sizing();
        }

        /// adds padding and borders to the content measurement, keeping at least one configured text line in height.
        void set_measured_content_size(ImVec2 size, bool measured_width, bool measured_height);

        void draw_surface(ImDrawList& draw_list, Rect rect, const std::optional<Color>& background = {}) const;

    private:
        static void style_changed(void* owner, bool font_changed);

        static void invalidate_font_cache_subtree(Node& node);

        void update_cursor();

        VisualState m_state;
        std::string_view m_type_name;
        std::unique_ptr<PaintSlot> m_before;
        std::unique_ptr<PaintSlot> m_after;
        mutable ImFont* m_cached_font = nullptr;
        mutable bool m_font_cache_valid = false;
    };
} // namespace ui
