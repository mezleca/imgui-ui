#pragma once

#include "../input/event.hpp"
#include "../input/router.hpp"
#include "../layout/geometry.hpp"

#include <memory>
#include <functional>
#include <cstdint>
#include <string>
#include <string_view>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ui {
    class Profiler;

    struct InputState {
        bool hovered = false;
        bool active = false;
        bool focused = false;
    };

    class Node {
    public:
        explicit Node(std::string id = {});
        Node(const Node&) = delete;
        virtual ~Node() = default;
        Node& operator=(const Node&) = delete;

        /// transfers ownership; rejects null nodes, cycles and children that already have a parent.
        bool add(std::unique_ptr<Node> child);

        template <typename T>
        T& add_child(std::unique_ptr<T> child) {
            if (child == nullptr || child.get() == this) {
                throw std::invalid_argument("a node cannot be added as its own child");
            }

            T* result = child.get();
            if (!add(std::move(child))) {
                throw std::logic_error("failed to add node child");
            }

            return *result;
        }

        template <typename T, typename... Args>
        T& add_child(Args&&... args) {
            return add_child(std::make_unique<T>(std::forward<Args>(args)...));
        }

        /// visible nodes advance framework state, call on_update(), then update descendants.
        void update(float dt);

        /// draws this node, then sends its final bounds to the input router when it has an input target.
        virtual void draw();

        /// detaches a direct child; input targets into its subtree are cleared.
        std::unique_ptr<Node> remove(Node& child);

        /// destroys every child after clearing input targets into their subtrees.
        void clear();

        /// attaches the surface input router to this node and its descendants.
        void set_input_router(InputRouter* router);

        /// attaches the profiler to this node and its descendants.
        void set_profiler(Profiler* profiler);

        /// returns the first node with the requested id in this subtree.
        Node* find(std::string_view id);

        /// const overload of find().
        const Node* find(std::string_view id) const;

        /// returns true when this node is the same as or contains the target.
        bool contains(const Node* node) const;

        /// returns the topmost visible selectable node at position, using the current draw order.
        Node* debug_node_at(ImVec2 position);

        /// returns whether this node has visible content the debugger can select on the canvas.
        virtual bool debug_selectable() const {
            return false;
        }

        const std::string& id() const {
            return m_id;
        }

        virtual std::string_view type_name() const {
            return "Node";
        }

        /// stable process-local identity used by diagnostics, independent from id().
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

        /// controls update and drawing independently from enabled input state.
        void set_visible(bool visible);

        bool enabled() const {
            return m_enabled;
        }

        /// controls input acceptance without hiding the node or stopping updates.
        void set_enabled(bool enabled) {
            m_enabled = enabled;
        }

        virtual bool accepts_input() const {
            return m_visible && m_enabled;
        }

        const InputState& input_state() const {
            return m_input_state;
        }

        /// sends pointer events inside this node's final bounds to this node after drawing.
        /// config.rect replaces those bounds and starts at this node's top-left.
        Node& set_input_target(RegionConfig config = {});

        /// consumes pointer events inside this node's final bounds unless a descendant is the target.
        Node& set_input_blocker(RegionConfig config = {});

        /// stops this node from registering its target or blocker after drawing.
        Node& clear_input_target();

        bool accepts_focus() const {
            return m_accepts_focus;
        }

        /// allows pointer activation to assign keyboard focus to this node.
        Node& set_accepts_focus(bool accepts_focus) {
            m_accepts_focus = accepts_focus;
            return *this;
        }

        /// returns the geometry resolved by the most recent draw pass.
        const NodeLayout& layout() const {
            return m_layout;
        }

        /// sets the requested outer size; non-positive axes are resolved by containers.
        Node& set_size(ImVec2 size) {
            if (m_layout.m_has_size_request && m_layout.requested_size().x == size.x && m_layout.requested_size().y == size.y) {
                return *this;
            }

            m_layout.set_size(size);
            invalidate_measure();
            return *this;
        }

        /// propagates measurement invalidation to ancestors.
        void invalidate_measure();

        /// places the node by aligning its top-left origin to the anchor point.
        Node& set_anchor(Anchor anchor) {
            m_layout.set_anchor(anchor);
            return *this;
        }

        /// places the node using a custom normalized parent anchor.
        Node& set_anchor_position(ImVec2 position) {
            m_layout.set_anchor_position(position);
            return *this;
        }

        /// chooses which normalized point of the node aligns with its anchor.
        Node& set_origin(Origin origin) {
            m_layout.set_origin(origin);
            return *this;
        }

        /// chooses a custom normalized point of the node for alignment.
        Node& set_origin_position(ImVec2 position) {
            m_layout.set_origin_position(position);
            return *this;
        }

        /// adds a local translation after anchor and origin alignment.
        Node& set_offset(ImVec2 offset) {
            m_layout.set_offset(offset);
            return *this;
        }

        /// sets anchor, origin and offset in one explicit placement operation.
        Node& set_placement(Placement placement) {
            m_layout.set_placement(placement);
            return *this;
        }

        /// returns placement to the current ImGui flow cursor.
        Node& set_flow() {
            m_layout.clear_explicit_position();
            return *this;
        }

    protected:
        /// internal event behavior implemented by the node itself.
        std::function<void(UiEvent&)> _on_event;

        /// dispatches the node's internal event behavior. widgets extend this with their public callback.
        virtual void dispatch_event(UiEvent& event);

        /// resolves flow or explicit placement and records local and screen bounds.
        void resolve_position();

        /// returns the screen-space bounds registered when set_input_target() has no config.rect.
        virtual Rect input_target_rect(Rect screen_rect) const {
            return screen_rect;
        }

        virtual void input_state_changed() {}

        /// reports whether set_size() supplied a requested size.
        bool has_size_request() const;

        /// reports whether a container resolved this node's size during layout.
        bool size_was_resolved() const;

        /// returns the size requested by the node before container resolution.
        ImVec2 requested_size() const;

        /// returns a child's requested size without changing its layout state.
        ImVec2 requested_size_of(const Node& child) const;

        /// writes the final width and height before draw
        /// containers call this when a requested zero axis must use the measured or available space.
        void resolve_size(ImVec2 size);

        /// returns the size a parent should use when arranging this node.
        virtual ImVec2 requested_size_for_layout() const {
            return requested_size();
        }

        /// overrides this node's screen bounds for custom drawing.
        void set_screen_rect(Rect rect);

        /// assigns a resolved size and explicit placement before the child draws.
        /// the child computes its screen bounds when its own draw pass starts.
        void arrange_child(Node& child, ImVec2 size, Anchor anchor, Origin origin, ImVec2 offset = {});

        bool capture_pointer();
        void release_pointer();

        void invalidate_measure_subtree();

        virtual void on_update(float dt);
        virtual void advance_frame_state(float dt);

        /// updates content-dependent state after children are measured.
        /// this callback must not depend on the current imgui window or cursor.
        virtual void on_measure();

        /// resolves dynamic size and placement immediately before drawing.
        /// override this for imgui-dependent layout values, not rendering.
        virtual void on_layout();

        /// opens the draw scope. returning false skips children, draw_after(), and on_draw_end().
        virtual bool on_draw();

        /// draws an optional decoration behind this node's contents.
        virtual void draw_before();

        /// draws children while the scope opened by on_draw() remains active.
        virtual void draw_children();

        /// draws an optional decoration above this node's contents while its draw scope remains active.
        virtual void draw_after();

        /// finalizes drawing and closes any scope opened by on_draw().
        virtual void on_draw_end();

        void set_input_state(InputState state);

        Profiler* profiler() const {
            return m_profiler;
        }

    private:
        friend class InputRouter;

        struct NodeRegion {
            bool blocks_input = false;
            RegionConfig config;
        };

        void measure_tree();
        Node& configure_region(bool blocks_input, RegionConfig config);
        void capture_leaf_rect(ImGuiID previous_item_id, Rect previous_item_rect);
        void register_input_target();
        void refresh_root_hover();

        /// public node key
        std::string m_id;

        // immutable runtime key
        // unlike id(), this does not need to be unique or user supplied.
        uint64_t m_identity = 0;
        // custom containers may draw children in an order unrelated to storage order.
        uint64_t m_draw_order = 0;

        Node* m_parent = nullptr;
        std::vector<std::unique_ptr<Node>> m_children;
        bool m_visible = true;
        bool m_enabled = true;
        bool m_accepts_focus = false;
        bool m_measure_dirty = true;
        NodeLayout m_layout;
        InputRouter* m_input_router = nullptr;
        Profiler* m_profiler = nullptr;
        std::optional<NodeRegion> m_region;
        InputState m_input_state;
    };

} // namespace ui
