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
        X = 1U << 0U,
        Y = 1U << 1U,
        Both = X | Y,
    };

    enum class LayoutSizeMode : uint8_t {
        Fixed,
        Percent,
        Fit,
        Grow,
    };

    struct LayoutAxis {
        float intrinsic(float measured) const;
        float resolve(float measured, float available) const;
        constexpr bool operator==(const LayoutAxis&) const = default;

        LayoutSizeMode mode = LayoutSizeMode::Grow;
        float value = 1.0F;
    };

    struct LayoutSize {
        constexpr bool operator==(const LayoutSize&) const = default;

        LayoutAxis width{};
        LayoutAxis height{};
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
        bool operator==(const Placement& other) const;

        Anchor anchor = Anchor::TopLeft;
        Anchor origin = Anchor::TopLeft;
        ImVec2 offset;
        ImVec2 anchor_position;
        ImVec2 origin_position;
    };

    struct LayoutConfig {
        bool operator==(const LayoutConfig& other) const {
            return size == other.size && placement == other.placement && in_flow == other.in_flow;
        }

        LayoutSize size{};
        Placement placement{};
        bool in_flow = true;
    };

    struct Rect {
        /// returns false for empty or inverted bounds.
        bool valid() const {
            return max.x > min.x && max.y > min.y;
        }

        ImVec2 size() const {
            return {max.x - min.x, max.y - min.y};
        }

        Rect inset(ImVec2 padding) const {
            return {{min.x + padding.x, min.y + padding.y}, {max.x - padding.x, max.y - padding.y}};
        }

        /// tests a point against the inclusive bounds.
        bool contains(ImVec2 point) const {
            return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
        }

        static Rect from_position_size(ImVec2 position, ImVec2 size) {
            return {position, {position.x + size.x, position.y + size.y}};
        }

        ImVec2 min;
        ImVec2 max;
    };

    struct BoxInsets {
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

        float left = 0.0F;
        float top = 0.0F;
        float right = 0.0F;
        float bottom = 0.0F;
    };

    ImVec2 alignment_factor(Anchor alignment);
    ImVec2 clamp_position(Rect bounds, ImVec2 size, ImVec2 position);
    Rect viewport_work_area();

    constexpr ResizeAxes operator&(ResizeAxes left, ResizeAxes right) {
        return static_cast<ResizeAxes>(static_cast<uint8_t>(left) & static_cast<uint8_t>(right));
    }

    class NodeLayout {
    public:
        /// outer allocation, including padding and borders.
        const ImVec2& size() const {
            return m_size;
        }

        const LayoutConfig& config() const {
            return m_config;
        }

        const LayoutSize& size_spec() const {
            return m_config.size;
        }

        /// returns the placement before a container arranges the node.
        const Placement& placement() const {
            return m_config.placement;
        }

        bool in_flow() const {
            return m_config.in_flow;
        }

        ImVec2 measured_size() const {
            return m_measured_size;
        }

        /// grow contributes its measured extent. percent contributes zero until parent space is available.
        ImVec2 intrinsic_size() const;

        /// percent contributes zero so a fit parent does not depend on its own unresolved allocation.
        ImVec2 preferred_size() const;

        ImVec2 resolved_size() const {
            return resolve_size(m_available_size);
        }

        ImVec2 resolve_size(ImVec2 available_size) const;

        const BoxInsets& box_insets() const {
            return m_box_insets;
        }

        /// cursor-relative bounds in the current imgui window.
        Rect local_rect() const {
            return m_local_rect;
        }

        /// arranged screen bounds before paint replaces the visual bounds.
        Rect layout_rect() const {
            return m_layout_rect;
        }

        /// screen bounds emitted by paint and read by pointer hit testing.
        Rect visual_rect() const {
            return m_visual_rect;
        }

        /// unscrolled content bounds in window-local coordinates.
        const Rect& parent_content_rect() const {
            return m_parent_content_rect;
        }

        const ImVec2& available_size() const {
            return m_available_size;
        }

    private:
        friend class Node;

        void set_size(LayoutSize size);
        void set_config(LayoutConfig config);
        void set_measured_size(ImVec2 size, bool measured_width, bool measured_height);
        void set_box_insets(BoxInsets insets);
        void set_box_sizing(BoxSizing sizing);

        void assign_size(ImVec2 size, bool assigned_by_parent = false);
        void clear_size_assignment();
        void invalidate_resolved_size();

        void set_arranged_placement(Placement placement) {
            m_arranged_placement = placement;
            m_has_arranged_position = true;
        }

        bool has_position() const {
            return !m_config.in_flow || m_has_arranged_position;
        }

        const Placement& active_placement() const {
            return m_has_arranged_position ? m_arranged_placement : m_config.placement;
        }

        void set_arranged_rects(Rect local_rect, Rect layout_rect);
        void set_layout_rect(Rect rect) {
            m_layout_rect = rect;
        }

        void set_visual_rect(Rect rect) {
            m_visual_rect = rect;
        }

        void set_parent_content_rect(Rect rect, ImVec2 available_size = {}) {
            m_parent_content_rect = rect;
            m_available_size = available_size;
        }

        static float intrinsic_axis(LayoutAxis axis, float measured, float insets, BoxSizing box_sizing);
        static float resolved_axis(LayoutAxis axis, float measured, float available, float insets, BoxSizing box_sizing);
        static float preferred_axis(LayoutAxis axis, float measured, float insets, BoxSizing box_sizing);

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
