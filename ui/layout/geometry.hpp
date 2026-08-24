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

    /// origin uses the same normalized points as anchor, but on the child side.
    using Origin = Anchor;

    struct Rect {
        ImVec2 min{};
        ImVec2 max{};

        bool valid() const {
            return max.x > min.x && max.y > min.y;
        }

        ImVec2 size() const {
            return {max.x - min.x, max.y - min.y};
        }

        bool contains(ImVec2 point) const {
            return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
        }

        static Rect from_position_size(ImVec2 position, ImVec2 size) {
            return {position, {position.x + size.x, position.y + size.y}};
        }
    };

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

    inline ImVec2 resolve_layout_position(
        ImVec2 parent_size, ImVec2 child_size, ImVec2 anchor_factor, ImVec2 origin_factor, ImVec2 offset = {}
    ) {
        return {
            parent_size.x * anchor_factor.x - child_size.x * origin_factor.x + offset.x,
            parent_size.y * anchor_factor.y - child_size.y * origin_factor.y + offset.y,
        };
    }

    inline ImVec2
    resolve_layout_position(ImVec2 parent_size, ImVec2 child_size, Anchor anchor, Origin origin, ImVec2 offset = {}) {
        return resolve_layout_position(parent_size, child_size, alignment_factor(anchor), alignment_factor(origin), offset);
    }

    inline Rect
    resolve_layout_rect(Rect parent, ImVec2 child_size, ImVec2 anchor_factor, ImVec2 origin_factor, ImVec2 offset = {}) {
        const ImVec2 position = resolve_layout_position(parent.size(), child_size, anchor_factor, origin_factor, offset);
        return Rect::from_position_size({parent.min.x + position.x, parent.min.y + position.y}, child_size);
    }

    /// non-positive requested dimensions consume the corresponding available dimension.
    inline ImVec2 resolve_layout_size(ImVec2 requested_size, ImVec2 available_size) {
        return {
            requested_size.x > 0.0F ? requested_size.x : std::max(0.0F, available_size.x),
            requested_size.y > 0.0F ? requested_size.y : std::max(0.0F, available_size.y),
        };
    }

    constexpr ResizeAxes operator&(ResizeAxes left, ResizeAxes right) {
        return static_cast<ResizeAxes>(static_cast<uint8_t>(left) & static_cast<uint8_t>(right));
    }

    class NodeLayout {
    public:
        /// size resolved for the current frame.
        const ImVec2& size() const {
            return m_size;
        }

        bool has_explicit_position() const {
            return m_has_explicit_position;
        }

        Anchor anchor() const {
            return m_anchor;
        }

        Origin origin() const {
            return m_origin;
        }

        ImVec2 anchor_factor() const {
            return m_anchor == Anchor::Custom ? m_anchor_position : alignment_factor(m_anchor);
        }

        ImVec2 origin_factor() const {
            return m_origin == Origin::Custom ? m_origin_position : alignment_factor(m_origin);
        }

        const ImVec2& offset() const {
            return m_offset;
        }

        ImVec2 arranged_position() const {
            return m_arranged_rect.min;
        }

        /// rectangle in coordinates local to the current imgui window.
        Rect arranged_rect() const {
            return m_arranged_rect;
        }

        /// latest outer bounds in screen coordinates.
        Rect screen_rect() const {
            return m_screen_rect;
        }

        /// parent content bounds used to resolve explicit placement.
        const Rect& parent_content_rect() const {
            return m_parent_content_rect;
        }

    private:
        friend class Node;

        void set_size(ImVec2 size) {
            m_requested_size = size;
            m_size = size;
            m_has_size_request = true;
            m_size_resolved = false;
        }

        void set_anchor(Anchor anchor) {
            m_anchor = anchor;
            m_has_explicit_position = true;
        }

        void set_anchor_position(ImVec2 position) {
            m_anchor = Anchor::Custom;
            m_anchor_position = position;
            m_has_explicit_position = true;
        }

        void set_origin(Origin origin) {
            m_origin = origin;
            m_has_explicit_position = true;
        }

        void set_origin_position(ImVec2 position) {
            m_origin = Origin::Custom;
            m_origin_position = position;
            m_has_explicit_position = true;
        }

        void set_offset(ImVec2 offset) {
            m_offset = offset;
            m_has_explicit_position = true;
        }

        void set_placement(Anchor anchor, Origin origin, ImVec2 offset) {
            m_anchor = anchor;
            m_origin = origin;
            m_offset = offset;
            m_has_explicit_position = true;
        }

        void clear_explicit_position() {
            m_has_explicit_position = false;
        }

        void set_arranged_rect(Rect rect) {
            m_arranged_rect = rect;
        }

        void set_resolved_size(ImVec2 size) {
            m_size = size;
            m_size_resolved = true;
        }

        const ImVec2& requested_size() const {
            return m_requested_size;
        }

        void clear_size_resolution() {
            m_size_resolved = false;
        }

        void set_screen_rect(Rect rect) {
            m_screen_rect = rect;
        }

        void set_parent_content_rect(Rect rect) {
            m_parent_content_rect = rect;
        }

        ImVec2 m_requested_size = {};
        ImVec2 m_size = {};
        ImVec2 m_offset = {};
        ImVec2 m_anchor_position = {};
        ImVec2 m_origin_position = {};
        Rect m_arranged_rect{};
        Rect m_screen_rect{};
        Rect m_parent_content_rect{};
        Anchor m_anchor = Anchor::TopLeft;
        Origin m_origin = Origin::TopLeft;
        bool m_has_size_request = false;
        bool m_size_resolved = false;
        bool m_has_explicit_position = false;
    };

} // namespace ui
