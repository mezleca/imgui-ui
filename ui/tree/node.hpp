#pragma once

#include "../input/event.hpp"
#include "../input/layer.hpp"
#include "../layout/geometry.hpp"

#include <memory>
#include <functional>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ui {
    class InputRouter;
    class Profiler;

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

        /// runs measure, placement and draw lifecycle, then registers the input region.
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

        /// returns the inherited layer used by the input router, or Count when unassigned.
        InputLayer input_layer() const {
            return m_input_layer;
        }

        /// input layer assignment propagates to descendants.
        Node& set_input_layer(InputLayer layer) {
            assign_input_layer(layer);
            return *this;
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

        /// returns whether this node consumes pointer input outside its descendants.
        virtual bool blocks_pointer_input() const {
            return false;
        }

        /// returns the screen-space bounds where this node blocks pointer input.
        virtual Rect pointer_blocking_rect() const {
            return m_layout.screen_rect();
        }

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
        Node& set_placement(Anchor anchor, Origin origin, ImVec2 offset = {}) {
            m_layout.set_placement(anchor, origin, offset);
            return *this;
        }

        /// returns placement to the current ImGui flow cursor.
        Node& set_flow() {
            m_layout.clear_explicit_position();
            return *this;
        }

        /// optional text-like representation used by the debugger and generic tooling.
        virtual std::optional<std::string> content() const;

        virtual bool try_set_content(std::string content);

    protected:
        /// internal event behavior implemented by the node itself.
        std::function<void(UiEvent&)> _on_event;

        /// dispatches the node's internal event behavior. widgets extend this with their public callback.
        virtual void dispatch_event(UiEvent& event);

        /// resolves flow or explicit placement and records local and screen bounds.
        void resolve_position();
        void assign_input_layer(InputLayer layer);
        void update_pointer_blocker_registration();
        /// reports whether set_size() supplied a requested size.
        bool has_size_request() const;

        /// reports whether a container resolved this node's size during layout.
        bool size_was_resolved() const;

        /// returns the size requested by the node before container resolution.
        ImVec2 requested_size() const;

        /// returns a child's requested size without changing its layout state.
        ImVec2 requested_size_of(const Node& child) const;
        void resolve_size(ImVec2 size);

        /// overrides this node's screen bounds for custom drawing.
        void set_screen_rect(Rect rect);

        /// assigns a resolved size and explicit placement before the child draws.
        /// the child computes its screen bounds when its own draw pass starts.
        void arrange_child(Node& child, ImVec2 size, Anchor anchor, Origin origin, ImVec2 offset = {});

        /// places a child by its top-left corner in screen coordinates.
        void arrange_child_at_screen(Node& child, ImVec2 size, ImVec2 screen_position);

        /// overrides a child node's screen bounds when its drawing happens elsewhere.
        void set_child_screen_rect(Node& child, Rect rect);
        void invalidate_measure_subtree();

        virtual void on_update(float dt);
        virtual void advance_frame_state(float dt);

        /// updates content-dependent state after children are measured.
        /// this callback must not depend on the current imgui window or cursor.
        virtual void on_measure();

        /// resolves dynamic size and placement immediately before drawing.
        /// override this for imgui-dependent layout values, not rendering.
        virtual void on_layout();

        /// opens the draw scope. returning false skips children and on_draw_end().
        virtual bool on_draw();

        /// draws children while the scope opened by on_draw() remains active.
        virtual void draw_children();

        /// finalizes drawing and closes any scope opened by on_draw().
        virtual void on_draw_end();

    private:
        friend class InputRouter;

        void measure_tree();
        void capture_leaf_rect(ImGuiID previous_item_id, Rect previous_item_rect);

        std::string m_id;
        // immutable runtime key; unlike id(), this does not need to be unique or user supplied.
        uint64_t m_identity = 0;
        // non-owning; ownership always lives in the parent's child vector.
        Node* m_parent = nullptr;
        std::vector<std::unique_ptr<Node>> m_children;
        bool m_visible = true;
        bool m_enabled = true;
        bool m_accepts_focus = false;
        // dirty state propagates upward so a root draw can measure affected subtrees once.
        bool m_measure_dirty = true;
        InputLayer m_input_layer = InputLayer::Count;
        NodeLayout m_layout;
        // surface services are inherited when a node is attached and cleared on removal.
        InputRouter* m_input_router = nullptr;
        Profiler* m_profiler = nullptr;
    };

} // namespace ui
