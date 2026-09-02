#include "node.hpp"

#include "../diagnostics/profiler.hpp"
#include "../input/router.hpp"

#include <algorithm>
#include <utility>

using namespace ui;

static uint64_t next_node_id = 1;
static uint64_t next_draw_order = 1;

Node::Node(std::string id) : m_id(std::move(id)), m_identity(next_node_id++) {}

Node* Node::debug_node_at(ImVec2 position) {
    if (!m_visible) {
        return nullptr;
    }

    Node* topmost_child = nullptr;
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
        Node* child = it->get();
        if (child->m_draw_order != 0 && m_draw_order != 0 && child->m_draw_order <= m_draw_order) {
            continue;
        }

        if (Node* candidate = child->debug_node_at(position); candidate != nullptr &&
            (topmost_child == nullptr || candidate->m_draw_order > topmost_child->m_draw_order)) {
            topmost_child = candidate;
        }
    }

    if (topmost_child != nullptr) {
        return topmost_child;
    }

    if (!m_layout.screen_rect().contains(position)) {
        return nullptr;
    }

    return debug_selectable() ? this : nullptr;
}

void Node::set_input_state(InputState state) {
    if (m_input_state.hovered == state.hovered && m_input_state.active == state.active &&
        m_input_state.focused == state.focused) {
        return;
    }

    m_input_state = state;
    input_state_changed();
}

Node& Node::set_input_target(RegionConfig config) {
    return configure_region(false, std::move(config));
}

Node& Node::set_input_blocker(RegionConfig config) {
    return configure_region(true, std::move(config));
}

Node& Node::configure_region(bool blocks_input, RegionConfig config) {
    if (m_input_router != nullptr) {
        m_input_router->clear_region(*this);
    }

    m_region = NodeRegion{blocks_input, std::move(config)};
    return *this;
}

Node& Node::clear_input_target() {
    if (m_input_router != nullptr) {
        m_input_router->clear_region(*this);
    }

    m_region.reset();
    return *this;
}

void Node::dispatch_event(UiEvent& event) {
    if (_on_event) {
        _on_event(event);
    }
}

void Node::resolve_position() {
    if (ImGui::GetCurrentContext() == nullptr) {
        m_layout.set_arranged_rect(Rect::from_position_size(m_layout.offset(), m_layout.size()));
        m_layout.set_screen_rect(m_layout.arranged_rect());
        return;
    }

    // imgui reports these bounds with scrolling applied, but setcursorpos requires window-local coordinates before scrolling.
    const ImVec2 scroll = {ImGui::GetScrollX(), ImGui::GetScrollY()};
    const ImVec2 content_min = ImGui::GetWindowContentRegionMin();
    const ImVec2 content_max = ImGui::GetWindowContentRegionMax();
    const Rect parent_content = {
        {content_min.x + scroll.x, content_min.y + scroll.y},
        {content_max.x + scroll.x, content_max.y + scroll.y},
    };
    m_layout.set_parent_content_rect(parent_content);

    ImVec2 window_position = ImGui::GetCursorPos();
    if (m_layout.has_explicit_position()) {
        const Rect arranged_rect = resolve_layout_rect(
            parent_content, m_layout.size(), m_layout.anchor_factor(), m_layout.origin_factor(), m_layout.offset()
        );
        window_position = arranged_rect.min;
        ImGui::SetCursorPos(window_position);
    }

    const ImVec2 screen_position = ImGui::GetCursorScreenPos();

    m_layout.set_arranged_rect(Rect::from_position_size(window_position, m_layout.size()));
    m_layout.set_screen_rect(Rect::from_position_size(screen_position, m_layout.size()));
}

void Node::set_input_router(InputRouter* router) {
    m_input_router = router;
    for (const auto& child : m_children) {
        child->set_input_router(router);
    }
}

void Node::set_visible(bool visible) {
    if (m_visible == visible) {
        return;
    }

    m_visible = visible;
    if (!visible && m_input_router != nullptr) {
        m_input_router->clear_regions(*this);
    }
    invalidate_measure();
}

void Node::set_profiler(Profiler* profiler) {
    m_profiler = profiler;
    for (const auto& child : m_children) {
        child->set_profiler(profiler);
    }
}

bool Node::add(std::unique_ptr<Node> child) {
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

bool Node::has_size_request() const {
    return m_layout.m_has_size_request;
}

bool Node::size_was_resolved() const {
    return m_layout.m_size_resolved;
}

ImVec2 Node::requested_size() const {
    return m_layout.requested_size();
}

ImVec2 Node::requested_size_of(const Node& child) const {
    return child.requested_size_for_layout();
}

void Node::resolve_size(ImVec2 size) {
    m_layout.set_resolved_size(size);
}

void Node::set_screen_rect(Rect rect) {
    m_layout.set_screen_rect(rect);
}

void Node::arrange_child(Node& child, ImVec2 size, Anchor anchor, Origin origin, ImVec2 offset) {
    child.m_layout.set_resolved_size(size);
    child.m_layout.set_arranged_placement(anchor, origin, offset);
}

bool Node::capture_pointer() {
    return m_input_router != nullptr && m_input_router->capture_pointer(*this);
}

void Node::release_pointer() {
    if (m_input_router != nullptr) {
        m_input_router->release_pointer();
    }
}

std::unique_ptr<Node> Node::remove(Node& child) {
    const auto it = std::find_if(m_children.begin(), m_children.end(), [&child](const std::unique_ptr<Node>& candidate) {
        return candidate.get() == &child;
    });

    if (it == m_children.end()) {
        return nullptr;
    }

    if (m_input_router != nullptr) {
        // the router retains raw node pointers across frames, so clear every target that would outlive this detached subtree.
        m_input_router->clear_focus(child);
        m_input_router->release_pointer(child);
        m_input_router->clear_regions(child);
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

    // clear every raw router reference before destroying the subtree.
    if (m_input_router != nullptr) {
        m_input_router->clear_focus(*this);
        m_input_router->release_pointer(*this);
        m_input_router->clear_regions(*this);
    }

    m_children.clear();
    invalidate_measure();
}

Node* Node::find(std::string_view searched_id) {
    if (m_id == searched_id) {
        return this;
    }

    for (const auto& child : m_children) {
        if (Node* result = child->find(searched_id); result != nullptr) {
            return result;
        }
    }

    return nullptr;
}

const Node* Node::find(std::string_view searched_id) const {
    if (m_id == searched_id) {
        return this;
    }

    for (const auto& child : m_children) {
        if (const Node* result = child->find(searched_id); result != nullptr) {
            return result;
        }
    }

    return nullptr;
}

bool Node::contains(const Node* node) const {
    if (node == this) {
        return true;
    }

    for (const auto& child : m_children) {
        if (child->contains(node)) {
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

void Node::invalidate_measure() {
    m_measure_dirty = true;
    if (m_parent != nullptr && !m_parent->m_measure_dirty) {
        m_parent->invalidate_measure();
    }
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

void Node::capture_leaf_rect(ImGuiID previous_item_id, Rect previous_item_rect) {
    if (!m_children.empty() || ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    const Rect item_rect{ImGui::GetItemRectMin(), ImGui::GetItemRectMax()};
    // compare id and bounds because many imgui items use id zero; a node that draws nothing must not inherit the previous item.
    const bool same_item = ImGui::GetItemID() == previous_item_id && item_rect.min.x == previous_item_rect.min.x &&
                           item_rect.min.y == previous_item_rect.min.y && item_rect.max.x == previous_item_rect.max.x &&
                           item_rect.max.y == previous_item_rect.max.y;
    if (same_item) {
        return;
    }

    if (item_rect.valid()) {
        m_layout.set_screen_rect(item_rect);
    }
}

void Node::register_input_target() {
    if (m_input_router == nullptr || !m_region.has_value()) {
        return;
    }

    const Rect screen_rect = m_layout.screen_rect();
    if (!screen_rect.valid()) {
        return;
    }

    RegionConfig region = m_region->config;
    if (!region.rect.valid()) {
        region.rect = input_target_rect(screen_rect);
    } else {
        region.rect.min.x += screen_rect.min.x;
        region.rect.min.y += screen_rect.min.y;
        region.rect.max.x += screen_rect.min.x;
        region.rect.max.y += screen_rect.min.y;
    }

    if (ImGui::GetCurrentContext() != nullptr) {
        const ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const ImVec2 clip_min = draw_list->GetClipRectMin();
        const ImVec2 clip_max = draw_list->GetClipRectMax();
        region.rect.min.x = std::max(region.rect.min.x, clip_min.x);
        region.rect.min.y = std::max(region.rect.min.y, clip_min.y);
        region.rect.max.x = std::min(region.rect.max.x, clip_max.x);
        region.rect.max.y = std::min(region.rect.max.y, clip_max.y);
    }

    if (!region.rect.valid()) {
        return;
    }

    if (m_region->blocks_input) {
        m_input_router->register_blocker(*this, std::move(region));
    } else {
        m_input_router->register_region(*this, std::move(region));
    }
}

void Node::refresh_root_hover() {
    if (m_parent != nullptr || m_input_router == nullptr || ImGui::GetCurrentContext() == nullptr) {
        return;
    }

    m_input_router->refresh_pointer_state(ImGui::GetIO().MousePos);
}

void Node::measure_tree() {
    UI_PROFILE_NODE(m_profiler, "Node::measure", m_identity);

    if (!m_visible || !m_measure_dirty) {
        return;
    }

    // measure children first because container size can depend on child sizes resolved in this frame.
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

    m_draw_order = next_draw_order++;

    if (m_parent == nullptr && m_measure_dirty) {
        measure_tree();
    }

    // resolve layout before painting so every widget receives final local and screen rectangles from the active parent window.
    on_layout();
    m_layout.clear_size_resolution();
    resolve_position();

    // only registered input leaves need the final rect reported by imgui; passive leaves keep their resolved layout rect.
    const bool captures_item_rect = m_region.has_value() && m_children.empty() && ImGui::GetCurrentContext() != nullptr;
    const ImGuiID previous_item_id = captures_item_rect ? ImGui::GetItemID() : 0;
    const Rect previous_item_rect = captures_item_rect ? Rect{ImGui::GetItemRectMin(), ImGui::GetItemRectMax()} : Rect{};

    draw_before();
    if (!on_draw()) {
        if (m_layout.has_explicit_position() && ImGui::GetCurrentContext() != nullptr) {
            ImGui::Dummy({});
        }
        return;
    }

    if (captures_item_rect) {
        capture_leaf_rect(previous_item_id, previous_item_rect);
    }

    draw_children();
    draw_after();
    on_draw_end();

    register_input_target();
    refresh_root_hover();
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
