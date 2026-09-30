#pragma once

#include "../style/box-sizing.hpp"

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
        Both = X | Y,
    };

    enum class LayoutSizeMode : uint8_t {
        /// uses the configured size according to the node box sizing.
        Fixed,
        /// uses a percentage of available parent content space according to the node box sizing.
        Percent,
        /// uses the measured content size.
        Fit,
        /// consumes the available parent space.
        Grow,
    };

    struct LayoutAxis {
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

            if (mode == LayoutSizeMode::Percent) {
                return std::max(0.0F, available) * value / 100.0F;
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

        constexpr bool operator==(const LayoutSize&) const = default;
    };

    constexpr LayoutAxis px(float value) {
        return {LayoutSizeMode::Fixed, std::max(0.0F, value)};
    }

    /// returns an axis sized to a percentage in the inclusive 0–100 range of its available parent space.
    constexpr LayoutAxis percent(float value) {
        return {LayoutSizeMode::Percent, std::clamp(value, 0.0F, 100.0F)};
    }

    constexpr LayoutAxis grow(float weight = 1.0F) {
        return {LayoutSizeMode::Grow, weight > 0.0F ? weight : 1.0F};
    }

    constexpr LayoutAxis fit() {
        return {LayoutSizeMode::Fit, 0.0F};
    }

    struct Placement {
        Anchor anchor = Anchor::TopLeft;
        Anchor origin = Anchor::TopLeft;
        ImVec2 offset;
        ImVec2 anchor_position;
        ImVec2 origin_position;

        bool operator==(const Placement& other) const {
            return anchor == other.anchor && origin == other.origin && offset.x == other.offset.x && offset.y == other.offset.y &&
                   anchor_position.x == other.anchor_position.x && anchor_position.y == other.anchor_position.y &&
                   origin_position.x == other.origin_position.x && origin_position.y == other.origin_position.y;
        }
    };

    struct LayoutConfig {
        /// sizing mode and value requested for each axis.
        LayoutSize size{};
        /// anchor, origin, and offset used to position the node.
        Placement placement{};
        /// whether the parent includes this node in flow arrangement.
        bool in_flow = true;

        bool operator==(const LayoutConfig& other) const {
            return size == other.size && placement == other.placement && in_flow == other.in_flow;
        }
    };

    /// axis-aligned bounds in one coordinate space.
    struct Rect {
        ImVec2 min;
        ImVec2 max;

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

    struct BoxInsets {
        float left = 0.0F;
        float top = 0.0F;
        float right = 0.0F;
        float bottom = 0.0F;

        float horizontal() const {
            return left + right;
        }

        float vertical() const {
            return top + bottom;
        }

        float axis(bool horizontal_axis) const {
            return horizontal_axis ? horizontal() : vertical();
        }

        ImVec2 window_padding() const {
            return {std::max(left, right), std::max(top, bottom)};
        }

        constexpr bool operator==(const BoxInsets&) const = default;
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
            return {
                intrinsic_axis(m_config.size.width, m_measured_size.x, m_box_insets.horizontal(), m_box_sizing),
                intrinsic_axis(m_config.size.height, m_measured_size.y, m_box_insets.vertical(), m_box_sizing),
            };
        }

        /// returns the natural size used by a fit-sized parent.
        ImVec2 preferred_size() const {
            return {
                preferred_axis(m_config.size.width, m_measured_size.x, m_box_insets.horizontal(), m_box_sizing),
                preferred_axis(m_config.size.height, m_measured_size.y, m_box_insets.vertical(), m_box_sizing),
            };
        }

        /// resolves this node's size from its parent allocation.
        ImVec2 resolved_size() const {
            return resolve_size(m_available_size);
        }

        /// resolves this node's size against a content allocation.
        ImVec2 resolve_size(ImVec2 available_size) const {
            return {
                resolved_axis(m_config.size.width, m_measured_size.x, available_size.x, m_box_insets.horizontal(), m_box_sizing),
                resolved_axis(m_config.size.height, m_measured_size.y, available_size.y, m_box_insets.vertical(), m_box_sizing),
            };
        }

        const BoxInsets& box_insets() const {
            return m_box_insets;
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
            invalidate_resolved_size();
        }

        void set_config(LayoutConfig config) {
            const bool size_changed = m_config.size != config.size;
            m_config = config;
            m_has_explicit_size_request = m_has_explicit_size_request || size_changed;
            m_has_arranged_position = false;
            if (size_changed) {
                invalidate_resolved_size();
            }
        }

        void set_measured_size(ImVec2 size, bool measured_width, bool measured_height) {
            m_measured_size = size;
            if (!m_has_explicit_size_request) {
                if (measured_width) m_config.size.width = fit();
                if (measured_height) m_config.size.height = fit();
            }

            invalidate_resolved_size();
        }

        void set_box_insets(BoxInsets insets) {
            if (m_box_insets == insets) {
                return;
            }

            m_box_insets = insets;
            invalidate_resolved_size();
        }

        void set_box_sizing(BoxSizing sizing) {
            if (m_box_sizing == sizing) {
                return;
            }

            m_box_sizing = sizing;
            invalidate_resolved_size();
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
            m_size_assigned_by_parent = false;
        }

        void invalidate_resolved_size() {
            m_size = intrinsic_size();
            m_has_size = false;
            m_size_assigned_by_parent = false;
        }

        void set_visual_rect(Rect rect) {
            m_visual_rect = rect;
        }

        void set_parent_content_rect(Rect rect, ImVec2 available_size = {}) {
            m_parent_content_rect = rect;
            m_available_size = available_size;
        }

        static float intrinsic_axis(LayoutAxis axis, float measured, float insets, BoxSizing box_sizing) {
            if (axis.mode == LayoutSizeMode::Fixed) {
                return box_sizing == BoxSizing::ContentBox ? axis.value + insets : std::max(axis.value, insets);
            }

            if (axis.mode == LayoutSizeMode::Grow) return std::max(measured, insets);
            return axis.intrinsic(measured);
        }

        static float resolved_axis(LayoutAxis axis, float measured, float available, float insets, BoxSizing box_sizing) {
            const float resolved = axis.resolve(measured, available);
            if (axis.mode == LayoutSizeMode::Fixed || axis.mode == LayoutSizeMode::Percent) {
                return box_sizing == BoxSizing::ContentBox ? resolved + insets : std::max(resolved, insets);
            }

            if (axis.mode == LayoutSizeMode::Grow) return std::max(resolved, insets);

            return resolved;
        }

        static float preferred_axis(LayoutAxis axis, float measured, float insets, BoxSizing box_sizing) {
            return axis.mode == LayoutSizeMode::Percent ? 0.0F : intrinsic_axis(axis, measured, insets, box_sizing);
        }

        const Placement& active_placement() const {
            return m_has_arranged_position ? m_arranged_placement : m_config.placement;
        }

        LayoutConfig m_config{};
        ImVec2 m_measured_size;
        BoxInsets m_box_insets{};
        BoxSizing m_box_sizing = BoxSizing::ContentBox;
        ImVec2 m_size;
        Rect m_local_rect{};
        Rect m_layout_rect{};
        Rect m_visual_rect{};
        Rect m_parent_content_rect{};
        ImVec2 m_available_size;
        Placement m_arranged_placement{};
        bool m_has_explicit_size_request = false;
        bool m_has_size = false;
        bool m_size_assigned_by_parent = false;
        bool m_has_arranged_position = false;
    };

} // namespace ui
