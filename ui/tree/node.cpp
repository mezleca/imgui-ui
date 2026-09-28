#include "node.hpp"

#include "../diagnostics/profiler.hpp"
#include "../input/router.hpp"
#include "../ui.hpp"

#include <imgui_internal.h>

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>

using namespace ui;
static uint64_t next_node_id = 1;

template <typename NodeType>
static NodeType* find_node(NodeType& root, std::string_view id) {
    if (root.removal_pending()) {
        return nullptr;
    }

    if (root.id() == id) {
        return &root;
    }

    for (const auto& child : root.children()) {
        NodeType* result = find_node(*child, id);

        if (result != nullptr) {
            return result;
        }
    }

    return nullptr;
}

Node::Node(std::string id) : m_id(std::move(id)), m_identity(next_node_id++) {}

Node& Node::add(std::unique_ptr<Node> child) {
    if (child == nullptr) {
        throw std::invalid_argument("cannot add a null child");
    }

    Node& result = *child;
    // connect the parent and inherited services before applying the surface theme to the subtree.
    result.m_parent = this;
    result.set_surface(m_surface);
    result.set_input_router(m_input_router);
    result.set_profiler(m_profiler);
    if (m_surface != nullptr) result.apply_theme(m_surface->theme());

    m_children.emplace_back(std::move(child));
    refresh_input_state();
    invalidate_measure();
    return result;
}

Node& Node::set_size(LayoutSize size) {
    if (m_layout.m_has_explicit_size_request && m_layout.size_spec() == size) return *this;
    m_layout.set_size(size);
    invalidate_measure();
    return *this;
}

Node& Node::set_layout(LayoutConfig config) {
    if (m_layout.config() == config) return *this;

    // remeasure this node and its ancestors when sizing or flow changes.
    const bool measure_changed = m_layout.size_spec() != config.size || m_layout.in_flow() != config.in_flow;
    m_layout.set_config(config);
    if (measure_changed) invalidate_measure();
    return *this;
}

Node& Node::set_anchor(Anchor anchor, Anchor origin) {
    LayoutConfig config = m_layout.config();
    config.placement.anchor = anchor;
    config.placement.origin = origin;
    config.in_flow = false;
    return set_layout(config);
}

Node::~Node() {
    if (m_input_router != nullptr) m_input_router->detach(*this);
}

void Node::set_input_state(InputState state) {
    if (m_input_state == state) {
        return;
    }

    m_input_state = state;
    refresh_input_state();
}

void Node::refresh_input_state() {
    // combine direct input with the children's aggregates, then pass the changed aggregate to the parent.
    for (Node* node = this; node != nullptr; node = node->m_parent) {
        InputState state = node->m_input_state;
        for (const auto& child : node->m_children) {
            if (child->m_removal_pending) continue;

            state.hovered |= child->m_subtree_input_state.hovered;
            state.active |= child->m_subtree_input_state.active;
        }

        if (state == node->m_subtree_input_state) break;

        node->m_subtree_input_state = state;
        node->input_state_changed();
    }
}

UI& Node::surface() const {
    if (m_surface == nullptr) {
        throw std::logic_error("node is not attached to a UI surface");
    }

    return *m_surface;
}

EffectRegistry* Node::effect_registry() const {
    return m_surface == nullptr ? nullptr : &m_surface->effects();
}

void Node::set_surface(UI* surface) {
    m_surface = surface;
    invalidate_measure();
    for (const auto& child : m_children) {
        child->set_surface(surface);
    }
}

void Node::set_enabled(bool enabled) {
    if (m_enabled == enabled) {
        return;
    }

    m_enabled = enabled;
    if (!enabled && m_input_router != nullptr) m_input_router->clear_input_state(*this);
}

Node& Node::set_input_mode(InputMode mode, Rect area) {
    if (m_input_mode == mode && m_input_area.min.x == area.min.x && m_input_area.min.y == area.min.y &&
        m_input_area.max.x == area.max.x && m_input_area.max.y == area.max.y) {
        return *this;
    }

    if (m_input_router != nullptr) m_input_router->m_hit_test.erase(*this);

    m_input_area = area;
    m_input_mode = mode;
    return *this;
}

ImVec2 Node::layout_margin() const {
    return {};
}

void Node::dispatch_event(UiEvent& event) {
    this->event(event);

    if (removal_pending() || event.propagation_stopped) {
        return;
    }

    switch (event.type) {
        case EventType::KeyDown:
            key_press_event(event);
            break;
        case EventType::KeyUp:
            key_release_event(event);
            break;
        case EventType::PointerDown:
            mouse_press_event(event);
            break;
        case EventType::PointerUp:
            mouse_release_event(event);
            break;
        case EventType::PointerMove:
            mouse_move_event(event);
            break;
        case EventType::Scroll:
            wheel_event(event);
            break;
        default:
            break;
    }
}

void Node::resolve_position(bool at_cursor) {
    const ImVec2 size = m_layout.size();
    ImGuiContext* context = ImGui::GetCurrentContext();

    if (context == nullptr || context->CurrentWindow == nullptr ||
        (m_parent == nullptr && context->CurrentWindow->IsFallbackWindow)) {
        const Rect rect = Rect::from_position_size(m_layout.active_placement().offset, size);
        m_layout.set_arranged_rects(rect, rect);
        return;
    }

    // flow nodes keep imgui's cursor. arranged nodes resolve their anchor in the parent's content box.
    ImVec2 local_position = ImGui::GetCursorPos();
    if (m_layout.has_position() && !at_cursor) {
        const Rect parent_rect = m_layout.parent_content_rect();
        const Placement& placement = m_layout.active_placement();
        const ImVec2 anchor = placement.anchor == Anchor::Custom ? placement.anchor_position : alignment_factor(placement.anchor);
        const ImVec2 origin = placement.origin == Anchor::Custom ? placement.origin_position : alignment_factor(placement.origin);
        local_position = {
            parent_rect.min.x + (parent_rect.size().x * anchor.x) - (size.x * origin.x) + placement.offset.x,
            parent_rect.min.y + (parent_rect.size().y * anchor.y) - (size.y * origin.y) + placement.offset.y,
        };
        ImGui::SetCursorPos(local_position);
    }

    // local bounds drive imgui. screen bounds drive painting and input hit testing.
    m_layout.set_arranged_rects(
        Rect::from_position_size(local_position, size), Rect::from_position_size(ImGui::GetCursorScreenPos(), size)
    );
}

void Node::capture_parent_content() {
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr || context->CurrentWindow == nullptr ||
        (m_parent == nullptr && context->CurrentWindow->IsFallbackWindow)) {
        m_layout.set_parent_content_rect({});
        return;
    }

    // cursor start is unscrolled. the logical cursor already includes the window scroll offset.
    const ImVec2 scroll = {ImGui::GetScrollX(), ImGui::GetScrollY()};
    const ImVec2 cursor = ImGui::GetCursorPos();
    const ImVec2 start = ImGui::GetCursorStartPos();
    const ImVec2 available = ImGui::GetContentRegionAvail();

    // express both content edges in the same local space before a container arranges its children.
    m_layout.set_parent_content_rect(
        {{start.x + scroll.x, start.y + scroll.y}, {cursor.x + available.x, cursor.y + available.y}}, available
    );
}

void Node::set_input_router(InputRouter* router) {
    if (m_input_router == router) {
        return;
    }

    if (m_input_router != nullptr) {
        m_input_router->clear_input_state(*this);
        std::erase(m_input_router->m_attached_nodes, this);
    }

    m_input_router = router;
    if (m_input_router != nullptr) {
        m_input_router->m_attached_nodes.push_back(this);
    }

    for (const auto& child : m_children) {
        child->set_input_router(router);
    }
}

void Node::detach_input_router(InputRouter& router) {
    if (m_input_router != &router) return;
    m_input_router = nullptr;
    m_input_state = {};
    m_subtree_input_state = {};
}

void Node::set_visible(bool visible) {
    if (m_visible == visible) {
        return;
    }

    m_visible = visible;
    if (!visible && m_input_router != nullptr) {
        m_input_router->clear_input_state(*this);
    }

    refresh_input_state();
    invalidate_measure();
}

void Node::set_profiler(Profiler* profiler) {
    if (m_profiler == profiler) {
        return;
    }

    m_profiler = profiler;
    for (const auto& child : m_children) {
        child->set_profiler(profiler);
    }
}

bool Node::has_size() const {
    return m_layout.m_has_size;
}

void Node::assign_size(ImVec2 size) {
    m_layout.assign_size(size);
}

void Node::set_measured_size(ImVec2 size, bool measured_width, bool measured_height) {
    m_layout.set_measured_size(size, measured_width, measured_height);
}

ImVec2 Node::content_size(ImVec2 size) const {
    const BoxInsets insets = box_insets();
    return {
        std::max(0.0F, size.x - insets.horizontal()),
        std::max(0.0F, size.y - insets.vertical()),
    };
}

ImVec2 Node::outer_size(ImVec2 size) const {
    const BoxInsets insets = box_insets();
    return {size.x + insets.horizontal(), size.y + insets.vertical()};
}

Rect Node::content_rect(Rect rect) const {
    const BoxInsets insets = box_insets();
    return Rect::from_position_size({rect.min.x + insets.left, rect.min.y + insets.top}, content_size(rect.size()));
}

void Node::set_visual_rect(Rect rect) {
    m_layout.set_visual_rect(rect);
}

void Node::set_layout_rect(Rect rect) {
    m_layout.set_layout_rect(rect);
}

void Node::arrange_child(Node& child, ImVec2 size, Placement placement) {
    child.m_layout.assign_size(size, true);
    child.m_layout.set_arranged_placement(placement);
}

bool Node::capture_pointer() {
    return m_input_router != nullptr && m_input_router->capture_pointer(*this);
}

void Node::release_pointer() {
    if (m_input_router != nullptr) m_input_router->release_pointer();
}

void Node::register_scroll_target(Rect rect) {
    // register only wheel input so a scrollable container does not steal clicks from its children.
    if (m_input_router != nullptr && rect.valid()) {
        m_input_router->register_target(*this, rect, {}, EventMask::Scroll);
    }
}

bool Node::remove(Node& child) {
    if (child.m_parent != this) {
        return false;
    }

    if (!child.m_removal_pending) {
        child.m_removal_pending = true;
        refresh_input_state();
        invalidate_measure();
    }

    return true;
}

std::unique_ptr<Node> Node::detach(Node& child) {
    if (child.m_parent != this) {
        return nullptr;
    }

    const auto it = std::find_if(m_children.begin(), m_children.end(), [&child](const std::unique_ptr<Node>& candidate) {
        return candidate.get() == &child;
    });
    return it == m_children.end() ? nullptr : detach_child(static_cast<size_t>(it - m_children.begin()));
}

std::unique_ptr<Node> Node::detach_child(size_t index) {
    const auto it = m_children.begin() + static_cast<std::ptrdiff_t>(index);
    std::unique_ptr<Node> result = std::move(*it);
    m_children.erase(it);
    result->m_removal_pending = false;
    result->m_parent = nullptr;
    result->set_surface(nullptr);
    result->set_input_router(nullptr);
    result->set_profiler(nullptr);
    refresh_input_state();
    invalidate_measure();
    return result;
}

void Node::clear() {
    if (m_children.empty()) {
        return;
    }

    bool changed = false;
    for (const auto& child : m_children) {
        changed |= !child->m_removal_pending;
        child->m_removal_pending = true;
    }

    if (changed) {
        refresh_input_state();
        invalidate_measure();
    }
}

Node* Node::find(std::string_view searched_id) {
    return find_node(*this, searched_id);
}

const Node* Node::find(std::string_view searched_id) const {
    return find_node(*this, searched_id);
}

bool Node::contains(const Node* node) const {
    for (const Node* current = node; current != nullptr; current = current->m_parent) {
        if (current == this) {
            return true;
        }
    }

    return false;
}

void Node::update(float dt) {
    UI_PROFILE_NODE(m_profiler, "Node::update", m_identity);

    if (!m_visible || m_removal_pending) {
        return;
    }

    advance_frame_state(dt);
    if (!m_removal_pending) on_update(dt);
    if (m_removal_pending) {
        return;
    }

    // visit the children present after this node's update hook, removing pending children before their turn.
    size_t child_count = m_children.size();
    for (size_t index = 0; index < child_count && index < m_children.size();) {
        Node* child = m_children[index].get();
        if (child->m_removal_pending) {
            detach_child(index);
            --child_count;
            continue;
        }

        child->update(dt);
        if (m_removal_pending) {
            return;
        }

        ++index;
    }
}

void Node::apply_theme(const Theme& theme) {
    apply_theme_defaults(theme);
    for (const auto& child : m_children) {
        if (!child->m_removal_pending) {
            child->apply_theme(theme);
        }
    }
}

void Node::invalidate_measure() {
    m_layout.set_box_insets(box_insets());
    m_layout.set_box_sizing(box_sizing());
    m_measure_dirty = true;
    if (m_parent != nullptr && !m_parent->m_measure_dirty) m_parent->invalidate_measure();
}

void Node::measure_tree() {
    UI_PROFILE_NODE(m_profiler, "Node::measure", m_identity);

    if (!m_visible || m_removal_pending || !m_measure_dirty) {
        return;
    }

    // measure children first so containers use measurements from this frame.
    const size_t child_count = m_children.size();
    for (size_t index = 0; index < child_count && index < m_children.size(); ++index) {
        Node* child = m_children[index].get();
        if (!child->m_removal_pending) {
            child->measure_tree();
        }
    }

    on_measure();
    m_measure_dirty = false;
}

void Node::draw() {
    draw_impl(false);
}

void Node::draw_at_cursor() {
    draw_impl(true);
}

void Node::draw_impl(bool at_cursor) {
    UI_PROFILE_NODE(m_profiler, "Node::draw", m_identity);

    if (!m_visible || m_removal_pending) {
        return;
    }

    if (m_profiler != nullptr) {
        m_profiler->record_node_draw();
    }

    // measure and arrange before paint so input uses the bounds produced by this draw.
    prepare_layout(at_cursor);
    draw_before();
    const bool draw_content = on_draw();

    if (draw_content) {
        draw_children();
        on_draw_end();
        draw_after();
    }

    submit_positioned_item(at_cursor);

    if (!draw_content || m_input_router == nullptr) return;

    UI_PROFILE_NODE(m_profiler, "Node::input", m_identity);
    if (m_input_mode != InputMode::None) {
        m_input_router->m_hit_test.register_node(*this, m_input_mode == InputMode::Blocker, m_input_area, m_layout.visual_rect());
    }

    if (m_parent == nullptr && ImGui::GetCurrentContext() != nullptr) {
        m_input_router->refresh_pointer_state(ImGui::GetIO().MousePos);
    }
}

void Node::submit_positioned_item(bool at_cursor) {
    if (!m_layout.has_position() || at_cursor) {
        return;
    }

    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr || context->CurrentWindow == nullptr || context->CurrentWindow->IsFallbackWindow ||
        !context->CurrentWindow->DC.IsSetPos) {
        return;
    }

    // imgui requires an item after moving the cursor when the position extends the parent bounds.
    ImGui::Dummy({});
}

void Node::prepare_layout(bool at_cursor) {
    UI_PROFILE_NODE(m_profiler, "Node::layout", m_identity);

    if (m_measure_dirty) {
        measure_tree();
    }

    // keep the size assigned by the parent while the node resolves its own placement.
    if (!m_layout.m_size_assigned_by_parent) {
        m_layout.clear_size_assignment();
    }

    capture_parent_content();
    on_layout();
    resolve_position(at_cursor);
    m_layout.clear_size_assignment();
}

void Node::draw_children() {
    for (const auto& child : m_children) {
        if (!child->m_removal_pending) {
            child->draw();
        }
    }
}

bool Node::on_draw() {
    return true;
}

void Node::draw_before() {}
void Node::on_update(float) {}
void Node::advance_frame_state(float) {}
void Node::on_measure() {}
void Node::on_layout() {
    if (!has_size()) assign_size(m_layout.resolved_size());
}
void Node::on_draw_end() {}
void Node::draw_after() {}

BoxInsets Node::box_insets() const {
    return {};
}

BoxSizing Node::box_sizing() const {
    return BoxSizing::ContentBox;
}
