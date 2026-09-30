#pragma once

#include <cstdint>
#include <imgui.h>
#include <optional>

struct ImGuiWindow;

namespace ui {
    class Container;

    enum class ScrollBehavior : uint8_t {
        /// uses the behavior configured on this Scroll instance.
        Default,
        Smooth,
        Instant,
    };

    /// tracks one container's scroll position and pending seek across frames.
    class Scroll {
    public:
        /// sets the behavior used by wheel input and seeks that request Default. starts as Smooth, and passing Default
        /// restores Smooth.
        Scroll& set_behaviour(ScrollBehavior behavior);
        /// queues an absolute offset, clamped to content bounds when the container is next drawn.
        void seek_to(ImVec2 position, ScrollBehavior behavior = ScrollBehavior::Default);
        /// queues an offset from the pending destination, or from the last drawn position when no seek is pending.
        void seek_by(ImVec2 distance, ScrollBehavior behavior = ScrollBehavior::Default);
        /// returns the offset observed during the most recent container draw.
        ImVec2 position() const {
            return m_position;
        }
        /// returns the scroll limit observed during the most recent container draw.
        ImVec2 max() const {
            return m_max;
        }

    private:
        friend class Container;

        void set_axes(bool vertical, bool horizontal);
        bool vertical() const {
            return m_vertical;
        }
        bool horizontal() const {
            return m_horizontal;
        }
        bool enabled() const {
            return m_vertical || m_horizontal;
        }
        bool wheel(ImVec2 delta);
        void advance(ImGuiWindow& window);

        struct Request {
            ImVec2 target;
            ImVec2 start;
            ScrollBehavior behavior;
            float elapsed = 0.0F;
        };

        ScrollBehavior m_behaviour = ScrollBehavior::Smooth;
        bool m_vertical = false;
        bool m_horizontal = false;
        std::optional<Request> m_request;
        ImVec2 m_position;
        ImVec2 m_max;
        ImVec2 m_wheel_step;
    };
} // namespace ui
