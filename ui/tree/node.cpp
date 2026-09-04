#include "node.hpp"

#include "../diagnostics/profiler.hpp"
#include "../input/router.hpp"

#include <imgui_internal.h>

#include <algorithm>
#include <utility>

using namespace ui;
static uint64_t next_node_id = 1;

template <typename NodeType>
static NodeType* find_node(NodeType& root, std::string_view id) {
    if (root.id() == id) {
        return &root;
    }

    for (const auto& child : root.children()) {
        if (NodeType* result = find_node(*child, id); result != nullptr) {
            return result;
        }
    }

    return nullptr;
}

Node::Node(std::string id) : m_id(std::move(id)), m_identity(next_node_id++) {}

Node::~Node() {
    if (m_input_router != nullptr) m_input_router->detach(*this);
}

void Node::set_input_state(InputState state) {
    if (m_input_state.hovered == state.hovered && m_input_state.active == state.active &&
        m_input_state.focused == state.focused) {
        return;
    }

    m_input_state = state;
    input_state_changed();
}

Node& Node::set_input_policy(InputPolicy policy, Rect area) {
    if (m_input_router != nullptr) m_input_router->erase_entries(*this);

    m_input_area = area;
    m_input_policy = policy;
    return *this;
}

Node& Node::set_input_target(Rect area) {
    return set_input_policy(InputPolicy::Target, area);
}

Node& Node::set_input_blocker(Rect area) {
    return set_input_policy(InputPolicy::Blocker, area);
}

Node& Node::clear_input() {
    return set_input_policy(InputPolicy::None, {});
}

void Node::dispatch_event(UiEvent& event) {
    if (_on_event) _on_event(event);
}

void Node::resolve_position() {
    const ImVec2 size = m_layout.size();

    // without an imgui context, use the requested offset as the local origin.
    if (ImGui::GetCurrentContext() == nullptr) {
        const Rect rect = Rect::from_position_size(m_layout.active_placement().offset, size);
        m_layout.set_arranged_rects(rect, rect);
        return;
    }

    // flow nodes keep the current cursor; absolute and arranged nodes resolve their anchor in parent content.
    ImVec2 local_position = ImGui::GetCursorPos();
    if (m_layout.has_position()) {
        const Rect arranged_rect = resolve_layout_rect(m_layout.parent_content_rect(), size, m_layout.active_placement());
        local_position = arranged_rect.min;
        ImGui::SetCursorPos(local_position);
    }

    // local_rect is the cursor-local box; layout_rect is the same box in screen coordinates.
    m_layout.set_arranged_rects(
        Rect::from_position_size(local_position, size), Rect::from_position_size(ImGui::GetCursorScreenPos(), size)
    );
}

void Node::capture_parent_content() {
    if (ImGui::GetCurrentContext() == nullptr) {
        m_layout.set_parent_content_rect({});
        return;
    }

    // cursor start is unscrolled; the logical cursor already includes the window scroll offset.
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
        clear_input_state();
        m_input_router->detach_node(*this);
    }

    m_input_router = router;
    if (m_input_router != nullptr) {
        m_input_router->attach_node(*this);
    }

    for (const auto& child : m_children) {
        child->set_input_router(router);
    }
}

void Node::detach_input_router(InputRouter& router) {
    if (m_input_router != &router) return;
    m_input_router = nullptr;
    m_input_state = {};
}

void Node::set_visible(bool visible) {
    if (m_visible == visible) {
        return;
    }

    m_visible = visible;
    if (!visible) {
        if (m_input_router != nullptr) m_input_router->clear_subtree_entries(*this);
    }

    invalidate_measure();
}

void Node::set_profiler(Profiler* profiler) {
    m_profiler = profiler;
    for (const auto& child : m_children) {
        child->set_profiler(profiler);
    }
}

bool Node::attach(std::unique_ptr<Node> child) {
    if (child == nullptr || child.get() == this || child->m_parent != nullptr || child->contains(this)) {
        return false;
    }

    child->m_parent = this;
    child->set_input_router(m_input_router);
    child->set_profiler(m_profiler);

    m_children.emplace_back(std::move(child));
    invalidate_measure();

    return true;
}

void Node::clear_input_state() {
    if (m_input_router == nullptr) return;

    m_input_router->clear_focus(*this);
    m_input_router->release_pointer(*this);
    m_input_router->clear_subtree_entries(*this);
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

std::unique_ptr<Node> Node::remove(Node& child) {
    const auto it = std::find_if(m_children.begin(), m_children.end(), [&child](const std::unique_ptr<Node>& candidate) {
        return candidate.get() == &child;
    });

    if (it == m_children.end()) {
        return nullptr;
    }

    std::unique_ptr<Node> result = std::move(*it);
    m_children.erase(it);
    result->m_parent = nullptr;
    result->set_input_router(nullptr);
    result->set_profiler(nullptr);
    invalidate_measure();
    return result;
}

void Node::clear() {
    if (m_children.empty()) {
        return;
    }

    m_children.clear();
    invalidate_measure();
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

    if (!m_visible) {
        return;
    }

    advance_frame_state(dt);
    on_update(dt);

    for (const auto& child : m_children) {
        child->update(dt);
    }
}

void Node::apply_theme(const Theme& theme) {
    apply_theme_defaults(theme);
    for (const auto& child : m_children) {
        child->apply_theme(theme);
    }
}

void Node::invalidate_measure() {
    m_measure_dirty = true;
    if (m_parent != nullptr && !m_parent->m_measure_dirty) m_parent->invalidate_measure();
}

void Node::invalidate_measure_subtree() {
    m_measure_dirty = true;
    for (const auto& child : m_children) {
        child->invalidate_measure_subtree();
    }

    if (m_parent != nullptr && !m_parent->m_measure_dirty) {
        m_parent->invalidate_measure();
    }
}

void Node::measure_tree() {
    UI_PROFILE_NODE(m_profiler, "Node::measure", m_identity);

    if (!m_visible || !m_measure_dirty) {
        return;
    }

    // measure children first so containers use measurements from this frame.
    for (const auto& child : m_children) {
        child->measure_tree();
    }

    on_measure();
    m_measure_dirty = false;
}

void Node::draw() {
    UI_PROFILE_NODE(m_profiler, "Node::draw", m_identity);

    if (!m_visible) {
        return;
    }

    if (m_profiler != nullptr) {
        m_profiler->record_node_draw();
    }

    // resolve layout, paint the subtree, then register input against the final visual rect.
    prepare_layout();
    draw_before();
    if (!on_draw()) {
        submit_positioned_item();
        return;
    }

    draw_children();
    draw_after();
    on_draw_end();
    submit_positioned_item();

    if (m_input_router != nullptr) {
        UI_PROFILE_NODE(m_profiler, "Node::input", m_identity);
        if (m_input_policy != InputPolicy::None) {
            m_input_router->register_node(*this, m_input_policy == InputPolicy::Blocker, m_input_area, m_layout.visual_rect());
        }

        if (m_parent == nullptr && ImGui::GetCurrentContext() != nullptr) {
            m_input_router->refresh_pointer_state(ImGui::GetIO().MousePos);
        }
    }
}

void Node::submit_positioned_item() {
    if (!m_layout.has_position()) {
        return;
    }

    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr || context->CurrentWindow == nullptr || !context->CurrentWindow->DC.IsSetPos) {
        return;
    }

    // imgui requires an item after SetCursorPos when the position extends the parent bounds.
    ImGui::Dummy({});
}

void Node::prepare_layout() {
    UI_PROFILE_NODE(m_profiler, "Node::layout", m_identity);

    if (m_parent == nullptr && m_measure_dirty) {
        measure_tree();
    }

    // keep the size assigned by the parent while the node resolves its own placement.
    const bool size_assigned_by_parent = m_layout.m_size_assigned_by_parent;
    if (!size_assigned_by_parent) {
        m_layout.clear_size_assignment();
    }

    capture_parent_content();
    on_layout();
    m_layout.clear_parent_size_assignment();
    resolve_position();
    m_layout.clear_size_assignment();
}

void Node::draw_children() {
    for (const auto& child : m_children) {
        child->draw();
    }
}

bool Node::on_draw() {
    return true;
}

void Node::draw_before() {}
void Node::on_update(float) {}
void Node::advance_frame_state(float) {}
void Node::on_measure() {}
void Node::on_layout() {}
void Node::on_draw_end() {}
void Node::draw_after() {}
