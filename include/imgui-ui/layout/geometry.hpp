#pragma once

#include "../style/box-sizing.hpp"

#include <imgui.h>
#include <algorithm>
#include <cstdint>
#include <limits>

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

    struct LayoutLimit {
        float resolve(float available) const;
        constexpr bool operator==(const LayoutLimit&) const = default;

        float value = 0.0F;
        bool percentage = false;
    };

    struct LayoutAxis {
        constexpr LayoutAxis() = default;
        constexpr LayoutAxis(LayoutSizeMode mode, float value) : mode(mode), value(value) {}

        /// grows between minimum and maximum, which must be px() or percent(). a resolved minimum above the
        /// maximum takes priority. {px(150), percent(50)} therefore never shrinks below 150 pixels.
        LayoutAxis(LayoutAxis minimum, LayoutAxis maximum);

        /// applies limits to an explicit rule, for example {fit(), px(10), px(100)} measures content within 10–100 pixels.
        LayoutAxis(LayoutAxis preferred, LayoutAxis minimum, LayoutAxis maximum);

        /// measures with pixel limits. percentage limits wait until parent space is available during resolve().
        float intrinsic(float measured, float insets = 0.0F, BoxSizing box_sizing = BoxSizing::BorderBox) const;

        /// resolves the rule and limits into an outer extent using available parent space in pixels.
        float resolve(float measured, float available, float insets = 0.0F, BoxSizing box_sizing = BoxSizing::BorderBox) const;
        float constrain(float size, float available, float insets = 0.0F, BoxSizing box_sizing = BoxSizing::BorderBox) const;
        constexpr bool operator==(const LayoutAxis&) const = default;

        LayoutSizeMode mode = LayoutSizeMode::Grow;
        float value = 1.0F;
        LayoutLimit minimum{};
        LayoutLimit maximum{std::numeric_limits<float>::infinity()};
    };

    struct LayoutSize {
        constexpr bool operator==(const LayoutSize&) const = default;

        LayoutAxis width;
        LayoutAxis height;
    };

    /// requests a fixed pixel extent, clamped to zero. ContentBox adds padding and borders outside this extent,
    /// while BorderBox includes them, with padding and borders as the minimum outer size.
    constexpr LayoutAxis px(float value) {
        return {LayoutSizeMode::Fixed, std::max(0.0F, value)};
    }

    /// percent(50) requests half the available parent space on this axis, not half the remainder after siblings.
    /// the percentage is clamped to [0, 100]. padding and borders follow the same box-sizing rule as px().
    constexpr LayoutAxis percent(float value) {
        return {LayoutSizeMode::Percent, std::clamp(value, 0.0F, 100.0F)};
    }

    /// fills available space across a stack. along its direction, growing children reserve their minimum, then
    /// share the remainder after other extents, margins and gaps. grow(2) receives twice the flexible share of grow(1).
    /// space left by children reaching their maximum is redistributed to those still below their maximum.
    /// nonpositive weights become 1. the weight does not affect allocation outside a stack's flow direction.
    constexpr LayoutAxis grow(float weight = 1.0F) {
        return {LayoutSizeMode::Grow, weight > 0.0F ? weight : 1.0F};
    }

    /// requests the node's measured outer extent. containers measure flowing children, margins and gaps,
    /// plus their own padding and borders. out-of-flow children do not enlarge a fit-sized container.
    constexpr LayoutAxis fit() {
        return {LayoutSizeMode::Fit, 0.0F};
    }

    struct Placement {
        bool operator==(const Placement& other) const;

        // aligns origin on the node with anchor in the parent's content box, then adds offset in pixels.
        // Custom reads anchor_position or origin_position as normalized factors instead of a named anchor.
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

        // true reserves stack space and contributes to parent fit measurements. false positions by placement
        // without reserving space. this does not change visibility, input eligibility or the node's own child layout.
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

        /// measures content with pixel limits applied. percentage rules and limits wait for parent allocation,
        /// avoiding a dependency on a fit-sized parent's own unresolved size.
        ImVec2 intrinsic_size() const;

        ImVec2 resolved_size() const {
            return resolve_size(m_available_size);
        }

        ImVec2 resolve_size(ImVec2 available_size) const;

        /// clamps an outer allocation against px/percent limits resolved from available parent space.
        /// padding and borders remain the minimum possible outer allocation. minimum wins over a smaller maximum.
        ImVec2 constrain_size(ImVec2 size, ImVec2 available_size) const;

        const BoxInsets& box_insets() const {
            return m_box_insets;
        }

        BoxSizing box_sizing() const {
            return m_box_sizing;
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
