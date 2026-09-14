#pragma once

#include "../input/event.hpp"
#include "../layout/geometry.hpp"

#include <concepts>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ui {
    class InputRouter;
    class HitTestIndex;
    class Profiler;
    class UI;
    struct Theme;

    struct InputState {
        bool hovered = false;
        bool active = false;
        bool focused = false;

        constexpr bool operator==(const InputState&) const = default;
    };

    enum class InputMode : uint8_t {
        /// does not register this node with the input router.
        None,
        /// registers the node as the normal event target for its hit rectangle.
        Target,
        /// consumes events in the hit rectangle before underlying targets resolve.
        Blocker,
    };

    /// Node coordinates lifecycle, layout, rendering, input registration, and propagation of the owning surface services to
    /// descendants.
    class Node {
    public:
        explicit Node(std::string id = {});
        Node(const Node&) = delete;
        virtual ~Node();
        Node& operator=(const Node&) = delete;

        /// takes ownership of a detached child.
        /// returns false for null, attached, or cyclic children.
        bool attach(std::unique_ptr<Node> child);

        /// constructs and owns a child.
        template <typename T, typename... Args>
        T& add(Args&&... args) {
            std::unique_ptr<T> child;
            if constexpr (std::constructible_from<T, Args...>) {
                child = std::make_unique<T>(std::forward<Args>(args)...);
            } else {
                static_assert(std::constructible_from<T, UI&, Args...>);
                child = std::make_unique<T>(surface(), std::forward<Args>(args)...);
            }

            T* result = child.get();
            if (!attach(std::move(child))) {
                throw std::logic_error("failed to add node child");
            }

            return *result;
        }

        /// updates this node and its visible descendants.
        virtual void update(float dt);

        /// reapplies theme defaults to this node and every descendant.
        void apply_theme(const Theme& theme);

        /// resolves, paints, and registers this visible subtree.
        virtual void draw();

        /// detaches a direct child.
        std::unique_ptr<Node> remove(Node& child);

        /// destroys all children and invalidates measurement.
        void clear();

        /// connects this subtree to a router.
        void set_input_router(InputRouter* router);

        /// records update and draw zones for this subtree.
        void set_profiler(Profiler* profiler);

        /// returns the first depth-first node with this id.
        /// returns null when absent.
        Node* find(std::string_view id);

        /// returns the first depth-first node with this id.
        /// returns null when absent.
        const Node* find(std::string_view id) const;

        /// returns true when node is this node or a descendant.
        bool contains(const Node* node) const;

        const std::string& id() const {
            return m_id;
        }

        virtual std::string_view type_name() const {
            return "Node";
        }

        /// returns the stable runtime identity.
        uint64_t identity() const {
            return m_identity;
        }

        void set_id(std::string id) {
            m_id = std::move(id);
        }

        Node* parent() {
            return m_parent;
        }

        const Node* parent() const {
            return m_parent;
        }

        const std::vector<std::unique_ptr<Node>>& children() const {
            return m_children;
        }

        bool visible() const {
            return m_visible;
        }

        /// hides this subtree and removes it from hit testing.
        void set_visible(bool visible);

        bool enabled() const {
            return m_enabled;
        }

        /// enables or disables input.
        void set_enabled(bool enabled);

        virtual bool accepts_input() const {
            return m_visible && m_enabled;
        }

        const InputState& input_state() const {
            return m_input_state;
        }

        /// returns direct focus plus hover and active state from the subtree.
        InputState subtree_input_state() const;

        /// configures this node's persistent input behavior. an empty area uses its visual box.
        Node& set_input_mode(InputMode mode, Rect area = {});

        /// returns geometry from the last draw pass.
        const NodeLayout& layout() const {
            return m_layout;
        }

        virtual ImVec2 layout_margin() const;

        /// replaces the width and height sizing modes.
        Node& set_size(LayoutSize size) {
            const LayoutSize& current = m_layout.size_spec();
            if (m_layout.m_has_explicit_size_request && current == size) {
                return *this;
            }

            m_layout.set_size(size);
            invalidate_measure_subtree();
            return *this;
        }

        /// replaces the complete layout request.
        Node& set_layout(LayoutConfig config) {
            if (m_layout.config() == config) {
                return *this;
            }

            const bool size_changed = m_layout.size_spec() != config.size;
            m_layout.set_config(config);
            if (size_changed) {
                invalidate_measure_subtree();
            } else {
                invalidate_measure();
            }
            return *this;
        }

        /// invalidates this node and its size-dependent ancestors.
        void invalidate_measure();

    protected:
        /// dispatches an event to this node.
        virtual void dispatch_event(UiEvent& event);

        /// handles the node's internal event behavior.
        virtual void on_event(UiEvent&) {}

        /// resolves placement and stores local and screen bounds.
        void resolve_position();

        /// returns hit bounds for visual_rect().
        virtual Rect hit_rect(Rect visual_rect) const {
            return visual_rect;
        }

        virtual void input_state_changed() {}

        bool has_size() const;

        /// stores the size assigned by a container.
        void assign_size(ImVec2 size);

        /// stores intrinsic size and measured axes.
        void set_measured_size(ImVec2 size, bool measured_width, bool measured_height);

        /// stores a measured content size after applying this node's line height and padding.
        void set_measured_content_size(ImVec2 size, bool measured_width, bool measured_height);
        ImVec2 content_size(ImVec2 size) const;
        ImVec2 outer_size(ImVec2 size) const;
        Rect content_rect(Rect rect) const;

        /// overrides visual bounds.
        void set_visual_rect(Rect rect);

        /// overrides arranged screen bounds.
        void set_layout_rect(Rect rect);

        /// assigns size and placement to a child.
        void arrange_child(Node& child, ImVec2 size, Placement placement = {});

        bool capture_pointer();
        void release_pointer();

        void invalidate_measure_subtree();

        virtual void on_update(float dt);
        virtual void advance_frame_state(float dt);

        /// resets built-in appearance values for the supplied theme.
        virtual void apply_theme_defaults(const Theme&) {}

        /// computes intrinsic size after children are measured.
        virtual void on_measure();

        /// resolves size and placement before paint.
        virtual void on_layout();

        /// paints this node and opens its child scope.
        /// returns false to skip children and post-paint hooks.
        virtual bool on_draw();

        /// paints an optional decoration before the node.
        virtual void draw_before();

        /// paints children in the current scope.
        virtual void draw_children();

        /// closes the node's paint scope.
        virtual void on_draw_end();

        /// paints an optional decoration above the completed node subtree.
        virtual void draw_after();

        virtual ImVec2 box_padding() const;
        virtual float minimum_content_height() const;

        void set_input_state(InputState state);

    private:
        friend class UI;
        friend class InputRouter;
        friend class HitTestIndex;

        UI& surface() const;
        void set_surface(UI* surface);
        void measure_tree();
        void detach_input_router(InputRouter& router);
        void clear_input_state();
        void capture_parent_content();
        void prepare_layout();
        void submit_positioned_item();

        std::string m_id;

        uint64_t m_identity = 0;
        Node* m_parent = nullptr;
        std::vector<std::unique_ptr<Node>> m_children;
        bool m_visible = true;
        bool m_enabled = true;
        bool m_measure_dirty = true;
        NodeLayout m_layout;
        UI* m_surface = nullptr;
        InputRouter* m_input_router = nullptr;
        Profiler* m_profiler = nullptr;
        Rect m_input_area{};
        InputMode m_input_mode = InputMode::None;
        InputState m_input_state;
    };

} // namespace ui
