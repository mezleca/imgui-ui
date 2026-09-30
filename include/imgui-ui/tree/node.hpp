#pragma once

#include "../input/event.hpp"
#include "../layout/geometry.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ui {
    class InputRouter;
    class HitTestIndex;
    class Profiler;
    class EffectRegistry;
    class Surface;
    struct Theme;

    struct InputState {
        bool hovered = false;
        bool active = false;
        bool focused = false;

        constexpr bool operator==(const InputState&) const = default;
    };

    enum class InputMode : uint8_t {
        /// default for plain nodes. the node's background is not a pointer target.
        /// descendants can register their own targets independently.
        None,
        /// default for widgets and containers. the hit rectangle receives pointer events, including empty space between children.
        /// events continue to underlying targets unless propagation is stopped.
        Target,
        /// restricts pointer routing to this subtree within the hit rectangle. underlying targets do not receive events.
        /// the last attached active blocker also restricts keyboard routing to its subtree.
        Blocker,
    };

    /// owns a subtree and runs its update, layout, paint, and input registration lifecycle.
    class Node {
    public:
        /// creates a detached node. parent is assigned only after construction, when another node takes ownership with add.
        explicit Node(std::string id = {});
        Node(const Node&) = delete;
        virtual ~Node();
        Node& operator=(const Node&) = delete;

        /// constructs and owns a child.
        template <typename T, typename... Args>
        T& add(Args&&... args) {
            return add(std::make_unique<T>(std::forward<Args>(args)...));
        }

        /// transfers ownership of a non-null child and connects its subtree to this parent. applies the theme when this
        /// parent belongs to a surface.
        Node& add(std::unique_ptr<Node> child);

        /// transfers ownership while preserving the child's concrete type in the returned reference.
        template <typename T>
        T& add(std::unique_ptr<T> child) {
            return static_cast<T&>(add(std::unique_ptr<Node>(std::move(child))));
        }

        /// updates this node and its visible descendants.
        virtual void update(float dt);

        /// reapplies theme defaults to this node and every descendant.
        void apply_theme(const Theme& theme);

        /// resolves, paints, and registers this visible subtree.
        virtual void draw();

        /// draws this node at the current imgui cursor instead of its arranged flow position.
        /// use it when a custom parent interleaves tree nodes or other native imgui items with framework children.
        void draw_at_cursor();

        /// marks a direct child for removal. the parent destroys it before its next update.
        bool remove(Node& child);

        /// immediately transfers ownership of a direct child. use remove() from tree callbacks.
        std::unique_ptr<Node> detach(Node& child);

        /// marks every child for removal.
        void clear();

        /// connects this subtree to a router.
        void set_input_router(InputRouter* router);

        /// records update and draw zones for this subtree.
        void set_profiler(Profiler* profiler);

        /// searches this node and its descendants by string id, skipping nodes pending removal. returns null when absent.
        Node* find(std::string_view id);

        /// searches this node and its descendants by string id, skipping nodes pending removal. returns null when absent.
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

        bool removal_pending() const {
            return m_removal_pending;
        }

        /// hides or shows this subtree. hiding stops updates and drawing and clears its hit regions immediately.
        void set_visible(bool visible);

        bool enabled() const {
            return m_enabled;
        }

        /// enables or disables input.
        void set_enabled(bool enabled);

        virtual bool accepts_input() const {
            return m_visible && m_enabled && !m_removal_pending && (m_parent == nullptr || m_parent->accepts_input());
        }

        const InputState& input_state() const {
            return m_input_state;
        }

        /// returns direct focus plus hover and active state from the subtree.
        InputState subtree_input_state() const {
            return m_subtree_input_state;
        }

        /// configures this node's persistent input behavior. a valid area is local to the node's visual rectangle
        /// and replaces hit_rect(). an empty area uses hit_rect().
        /// target receives pointer events without blocking underlying nodes. none leaves only descendant hit regions.
        /// registering a target does not give it keyboard focus. use input_router().set_focus() for that.
        Node& set_input_mode(InputMode mode, Rect area = {});

        /// returns geometry from the last draw pass.
        const NodeLayout& layout() const {
            return m_layout;
        }

        virtual ImVec2 layout_margin() const;

        /// replaces the width and height sizing modes interpreted by this node's box sizing style.
        Node& set_size(LayoutSize size);

        /// replaces the complete layout request.
        Node& set_layout(LayoutConfig config);

        /// positions this node at the matching point in its parent and removes it from flow layout.
        Node& set_anchor(Anchor anchor) {
            return set_anchor(anchor, anchor);
        }

        /// positions this node using explicit parent and node anchor points and removes it from flow layout.
        Node& set_anchor(Anchor anchor, Anchor origin);

        /// invalidates this node and its size-dependent ancestors.
        void invalidate_measure();

    protected:
        /// reconnects surface context and remeasures the subtree after attachment or detachment.
        virtual void set_surface(Surface* surface);
        /// returns the Surface that owns this attached node.
        Surface& surface() const;
        EffectRegistry* effect_registry() const;

        /// dispatches an event to this node.
        virtual void dispatch_event(UiEvent& event);

        /// runs first for every event reaching this node. stopping propagation skips the specific handler and remaining routing.
        virtual void event(UiEvent&) {}

        /// runs after event() for the matching event type, unless propagation was stopped or this node was removed.
        virtual void key_press_event(UiEvent&) {}
        virtual void key_release_event(UiEvent&) {}
        virtual void mouse_press_event(UiEvent&) {}
        virtual void mouse_release_event(UiEvent&) {}
        virtual void mouse_move_event(UiEvent&) {}
        virtual void wheel_event(UiEvent&) {}

        /// resolves placement and stores local and screen bounds.
        void resolve_position(bool at_cursor);

        /// returns the screen-space area used to select this node for pointer events. defaults to visual_rect.
        /// called after drawing when input mode is target or blocker and no explicit input area was supplied.
        /// the router clips the result to the current imgui clip. an invalid rectangle registers no target or blocker.
        /// override to restrict input to a child control or extend it to resize handles without changing layout bounds.
        virtual Rect hit_rect(Rect visual_rect) const {
            return visual_rect;
        }

        /// returns whether this node registers a framework input target.
        bool has_input_mode() const {
            return m_input_mode != InputMode::None;
        }

        /// returns true for direct children that own input. deeper containers decide their own pass-through window.
        bool has_direct_input_child() const {
            for (const auto& child : m_children) {
                if (!child->m_removal_pending && child->has_input_mode()) {
                    return true;
                }
            }
            return false;
        }

        virtual void input_state_changed() {}

        bool has_size() const;

        /// stores the size assigned by a container.
        void assign_size(ImVec2 size);

        /// stores intrinsic size and measured axes.
        void set_measured_size(ImVec2 size, bool measured_width, bool measured_height);

        ImVec2 content_size(ImVec2 size) const;
        ImVec2 outer_size(ImVec2 size) const;
        Rect content_rect(Rect rect) const;

        /// overrides visual bounds.
        void set_visual_rect(Rect rect);

        /// overrides arranged screen bounds.
        void set_layout_rect(Rect rect);

        /// assigns size and placement to a child.
        static void arrange_child(Node& child, ImVec2 size, Placement placement = {});

        bool capture_pointer();
        void release_pointer();
        void register_scroll_target(Rect rect);

        virtual void on_update(float dt);
        virtual void advance_frame_state(float dt);

        /// resets built-in appearance values for the supplied theme.
        virtual void apply_theme_defaults(const Theme&) {}

        /// computes intrinsic size after children are measured. parent allocation is resolved separately before paint.
        virtual void on_measure();

        /// runs before each paint. resolves size from available content space when the parent has not assigned it.
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

        virtual BoxInsets box_insets() const;
        virtual BoxSizing box_sizing() const;

        void set_input_state(InputState state);

    private:
        friend class Surface;
        friend class InputRouter;
        friend class HitTestIndex;

        void measure_tree();
        void detach_input_router(InputRouter& router);
        void refresh_input_state();
        std::unique_ptr<Node> detach_child(size_t index);
        void capture_parent_content();
        void draw_impl(bool at_cursor);
        void prepare_layout(bool at_cursor);
        void submit_positioned_item(bool at_cursor);

        std::string m_id;

        uint64_t m_identity = 0;
        Node* m_parent = nullptr;
        std::vector<std::unique_ptr<Node>> m_children;
        bool m_visible = true;
        bool m_enabled = true;
        bool m_measure_dirty = true;
        NodeLayout m_layout;
        Surface* m_surface = nullptr;
        InputRouter* m_input_router = nullptr;
        Profiler* m_profiler = nullptr;
        Rect m_input_area{};
        InputMode m_input_mode = InputMode::None;
        InputState m_input_state;
        InputState m_subtree_input_state;
        bool m_removal_pending = false;
    };

} // namespace ui
