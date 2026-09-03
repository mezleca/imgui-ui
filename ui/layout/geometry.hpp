#pragma once

#include <imgui.h>
#include <algorithm>
#include <cstdint>

namespace ui {
    enum class Anchor : uint8_t {
        TopLeft,
        TopCenter,
        TopRight,
        CenterLeft,
        Center,
        CenterRight,
        BottomLeft,
        BottomCenter,
        BottomRight,
        Custom,
    };

    enum class StackDirection : uint8_t {
        Horizontal,
        Vertical,
    };

    enum class ResizeAxes : uint8_t {
        None = 0,
        X = 1 << 0,
        Y = 1 << 1,
        Both = static_cast<uint8_t>(X) | static_cast<uint8_t>(Y),
    };

    using Origin = Anchor;

    enum class LayoutSizeMode : uint8_t {
        Fixed,
        Fit,
        Grow,
    };

    struct LayoutAxis {
        static constexpr LayoutAxis grow(float weight = 1.0F) {
            return {LayoutSizeMode::Grow, weight > 0.0F ? weight : 1.0F};
        }

        static constexpr LayoutAxis fixed(float value) {
            return {LayoutSizeMode::Fixed, std::max(0.0F, value)};
        }

        static constexpr LayoutAxis fit() {
            return {LayoutSizeMode::Fit, 0.0F};
        }

        float intrinsic(float measured) const {
            if (mode == LayoutSizeMode::Fixed) {
                return value;
            }

            return mode == LayoutSizeMode::Fit ? std::max(0.0F, measured) : 0.0F;
        }

        float resolve(float measured, float available) const {
            if (mode == LayoutSizeMode::Grow) {
                return std::max(0.0F, available);
            }

            return intrinsic(measured);
        }

        LayoutSizeMode mode = LayoutSizeMode::Grow;
        float value = 1.0F;

        constexpr bool operator==(const LayoutAxis&) const = default;
    };

    struct LayoutSize {
        LayoutAxis width{};
        LayoutAxis height{};

        ImVec2 intrinsic(ImVec2 measured) const {
            return {width.intrinsic(measured.x), height.intrinsic(measured.y)};
        }

        ImVec2 resolve(ImVec2 measured, ImVec2 available) const {
            return {width.resolve(measured.x, available.x), height.resolve(measured.y, available.y)};
        }

        constexpr bool operator==(const LayoutSize&) const = default;
    };

    constexpr LayoutAxis px(float value) {
        return LayoutAxis::fixed(value);
    }

    constexpr LayoutAxis grow(float weight = 1.0F) {
        return LayoutAxis::grow(weight);
    }

    constexpr LayoutAxis fit() {
        return LayoutAxis::fit();
    }

    struct Placement {
        Anchor anchor = Anchor::TopLeft;
        Origin origin = Origin::TopLeft;
        ImVec2 offset{};
        ImVec2 anchor_position{};
        ImVec2 origin_position{};
    };

    struct LayoutConfig {
        LayoutSize size{};
        Placement placement{};
        bool in_flow = true;
    };

    /// axis-aligned bounds in one coordinate space.
    struct Rect {
        ImVec2 min{};
        ImVec2 max{};

        /// returns false for empty or inverted bounds.
        bool valid() const {
            return max.x > min.x && max.y > min.y;
        }

        /// returns the width and height represented by the bounds.
        ImVec2 size() const {
            return {max.x - min.x, max.y - min.y};
        }

        /// returns bounds inset by the same amount on each side.
        Rect inset(ImVec2 padding) const {
            return {{min.x + padding.x, min.y + padding.y}, {max.x - padding.x, max.y - padding.y}};
        }

        /// tests a point against the inclusive bounds.
        bool contains(ImVec2 point) const {
            return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
        }

        /// constructs bounds from a top-left position and an extent.
        static Rect from_position_size(ImVec2 position, ImVec2 size) {
            return {position, {position.x + size.x, position.y + size.y}};
        }
    };

    /// converts a named anchor or origin to normalized coordinates.
    inline ImVec2 alignment_factor(Anchor alignment) {
        switch (alignment) {
            case Anchor::TopLeft:
                return {0.0F, 0.0F};
            case Anchor::TopCenter:
                return {0.5F, 0.0F};
            case Anchor::TopRight:
                return {1.0F, 0.0F};
            case Anchor::CenterLeft:
                return {0.0F, 0.5F};
            case Anchor::Center:
                return {0.5F, 0.5F};
            case Anchor::CenterRight:
                return {1.0F, 0.5F};
            case Anchor::BottomLeft:
                return {0.0F, 1.0F};
            case Anchor::BottomCenter:
                return {0.5F, 1.0F};
            case Anchor::BottomRight:
                return {1.0F, 1.0F};
            case Anchor::Custom:
                return {};
        }
        return {};
    }

    /// resolves a child top-left position inside a parent extent.
    inline ImVec2 resolve_layout_position(
        ImVec2 parent_size, ImVec2 child_size, ImVec2 anchor_factor, ImVec2 origin_factor, ImVec2 offset = {}
    ) {
        return {
            parent_size.x * anchor_factor.x - child_size.x * origin_factor.x + offset.x,
            parent_size.y * anchor_factor.y - child_size.y * origin_factor.y + offset.y,
        };
    }

    /// resolves a child top-left position from named anchor and origin points.
    inline ImVec2
    resolve_layout_position(ImVec2 parent_size, ImVec2 child_size, Anchor anchor, Origin origin, ImVec2 offset = {}) {
        return resolve_layout_position(parent_size, child_size, alignment_factor(anchor), alignment_factor(origin), offset);
    }

    /// resolves child bounds in the same coordinate space as the parent rectangle.
    inline Rect
    resolve_layout_rect(Rect parent, ImVec2 child_size, ImVec2 anchor_factor, ImVec2 origin_factor, ImVec2 offset = {}) {
        const ImVec2 position = resolve_layout_position(parent.size(), child_size, anchor_factor, origin_factor, offset);
        return Rect::from_position_size({parent.min.x + position.x, parent.min.y + position.y}, child_size);
    }

    /// resolves child bounds from a placement request.
    inline Rect resolve_layout_rect(Rect parent, ImVec2 child_size, const Placement& placement) {
        const ImVec2 anchor = placement.anchor == Anchor::Custom ? placement.anchor_position : alignment_factor(placement.anchor);
        const ImVec2 origin = placement.origin == Origin::Custom ? placement.origin_position : alignment_factor(placement.origin);
        return resolve_layout_rect(parent, child_size, anchor, origin, placement.offset);
    }

    inline ImVec2 clamp_position(Rect bounds, ImVec2 size, ImVec2 position) {
        return {
            std::clamp(position.x, bounds.min.x, std::max(bounds.min.x, bounds.max.x - size.x)),
            std::clamp(position.y, bounds.min.y, std::max(bounds.min.y, bounds.max.y - size.y)),
        };
    }

    /// tests which resize axes are enabled in both masks.
    constexpr ResizeAxes operator&(ResizeAxes left, ResizeAxes right) {
        return static_cast<ResizeAxes>(static_cast<uint8_t>(left) & static_cast<uint8_t>(right));
    }

    class NodeLayout {
    public:
        /// returns the final size assigned by the parent layout.
        const ImVec2& size() const {
            return m_size;
        }

        /// returns the node's requested size and placement.
        const LayoutConfig& config() const {
            return m_config;
        }

        /// returns the width and height sizing rules.
        const LayoutSize& size_spec() const {
            return m_config.size;
        }

        /// returns the placement before a container arranges the node.
        const Placement& placement() const {
            return m_config.placement;
        }

        /// returns whether the node participates in its parent's flow.
        bool in_flow() const {
            return m_config.in_flow;
        }

        /// returns the measured size before grow allocation.
        ImVec2 measured_size() const {
            return m_measured_size;
        }

        /// returns fixed and fit size without grow allocation.
        ImVec2 intrinsic_size() const {
            return m_config.size.intrinsic(m_measured_size);
        }

        /// returns the arranged bounds passed to the imgui cursor.
        Rect local_rect() const {
            return m_local_rect;
        }

        /// returns the arranged bounds in screen coordinates.
        Rect layout_rect() const {
            return m_layout_rect;
        }

        /// returns the bounds emitted by the node's paint operation.
        Rect visual_rect() const {
            return m_visual_rect;
        }

        /// unscrolled content bounds in window-local coordinates.
        const Rect& parent_content_rect() const {
            return m_parent_content_rect;
        }

        /// returns the remaining content space at the node's layout cursor.
        const ImVec2& available_size() const {
            return m_available_size;
        }

    private:
        friend class Node;

        void set_size(LayoutSize size) {
            m_config.size = size;
            m_has_explicit_size_request = true;
            m_size = intrinsic_size();
            m_has_size = false;
            m_size_assigned_by_parent = false;
        }

        void set_config(LayoutConfig config) {
            const bool size_changed = m_config.size != config.size;
            m_config = config;
            m_has_explicit_size_request = m_has_explicit_size_request || size_changed;
            m_has_arranged_position = false;
            m_size = intrinsic_size();
            m_has_size = false;
            m_size_assigned_by_parent = false;
        }

        void set_measured_size(ImVec2 size, bool measured_width, bool measured_height) {
            m_measured_size = size;
            if (!m_has_explicit_size_request) {
                if (measured_width) {
                    m_config.size.width = LayoutAxis::fit();
                }
                if (measured_height) {
                    m_config.size.height = LayoutAxis::fit();
                }
            }

            m_size = intrinsic_size();
            m_has_size = false;
            m_size_assigned_by_parent = false;
        }

        void set_arranged_placement(Placement placement) {
            m_arranged_placement = placement;
            m_has_arranged_position = true;
        }

        bool has_position() const {
            return !m_config.in_flow || m_has_arranged_position;
        }

        void set_arranged_rects(Rect local_rect, Rect layout_rect) {
            m_local_rect = local_rect;
            m_layout_rect = layout_rect;
            m_visual_rect = layout_rect;
        }

        void set_layout_rect(Rect rect) {
            m_layout_rect = rect;
        }

        void assign_size(ImVec2 size, bool assigned_by_parent = false) {
            m_size = size;
            m_has_size = true;
            m_size_assigned_by_parent = assigned_by_parent;
        }

        void clear_size_assignment() {
            m_has_size = false;
        }

        void clear_parent_size_assignment() {
            m_size_assigned_by_parent = false;
        }

        void set_visual_rect(Rect rect) {
            m_visual_rect = rect;
        }

        void set_parent_content_rect(Rect rect, ImVec2 available_size = {}) {
            m_parent_content_rect = rect;
            m_available_size = available_size;
        }

        const Placement& active_placement() const {
            return m_has_arranged_position ? m_arranged_placement : m_config.placement;
        }

        LayoutConfig m_config{};
        ImVec2 m_measured_size{};
        ImVec2 m_size = {};
        Rect m_local_rect{};
        Rect m_layout_rect{};
        Rect m_visual_rect{};
        Rect m_parent_content_rect{};
        ImVec2 m_available_size{};
        Placement m_arranged_placement{};
        bool m_has_explicit_size_request = false;
        bool m_has_size = false;
        bool m_size_assigned_by_parent = false;
        bool m_has_arranged_position = false;
    };

} // namespace ui
