#include "hit-test-index.hpp"

#include "../tree/node.hpp"

#include <imgui_internal.h>

#include <algorithm>
#include <utility>

using namespace ui;

void HitTestIndex::begin_frame(std::size_t expected_entries) {
    m_entries.clear();
    m_checks = 0;

    m_entries.reserve(expected_entries);
}

void HitTestIndex::add(Node* node, EntryKind kind, Rect rect, EventMask events, InputCallback callback) {
    // dispatch snapshots share the callable so region removal cannot destroy it and captured state persists between events.
    auto handler = callback ? std::make_shared<InputCallback>(std::move(callback)) : nullptr;
    m_entries.push_back(Entry{node, rect, std::move(handler), events, kind});
}

void HitTestIndex::register_node(Node& node, bool blocker, Rect input_rect, Rect visual_rect) {
    if (!visual_rect.valid()) {
        return;
    }

    if (!input_rect.valid()) {
        input_rect = node.hit_rect(visual_rect);
    } else {
        // explicit hit rectangles are local to the node's visual rectangle.
        input_rect.min.x += visual_rect.min.x;
        input_rect.min.y += visual_rect.min.y;
        input_rect.max.x += visual_rect.min.x;
        input_rect.max.y += visual_rect.min.y;
    }

    // read the current clip without marking imgui's fallback window as painted.
    if (ImGui::GetCurrentContext() != nullptr) {
        const ImDrawList* draw_list = ImGui::GetCurrentWindowRead()->DrawList;
        const Rect clip_rect = {draw_list->GetClipRectMin(), draw_list->GetClipRectMax()};
        input_rect.min.x = std::max(input_rect.min.x, clip_rect.min.x);
        input_rect.min.y = std::max(input_rect.min.y, clip_rect.min.y);
        input_rect.max.x = std::min(input_rect.max.x, clip_rect.max.x);
        input_rect.max.y = std::min(input_rect.max.y, clip_rect.max.y);
    }

    if (!input_rect.valid()) {
        return;
    }

    if (blocker) add(&node, EntryKind::Blocker, input_rect, EventMask::Pointer, {});

    add(&node, EntryKind::Target, input_rect, EventMask::Pointer, {});
}

void HitTestIndex::erase(Node& node) {
    std::erase_if(m_entries, [&node](const Entry& entry) { return entry.node == &node; });
}

void HitTestIndex::erase_subtree(Node& subtree) {
    std::erase_if(m_entries, [&subtree](const Entry& entry) { return subtree.contains(entry.node); });
}

HitTestIndex::Route HitTestIndex::route_at(ImVec2 position, EventType type) const {
    Route route;
    const EventMask mask = event_mask(type);
    // select the front blocker before collecting targets in its subtree.
    for (auto it = m_entries.rbegin(); it != m_entries.rend(); ++it) {
        ++m_checks;
        if (it->kind != EntryKind::Blocker || !it->rect.contains(position) || !contains(it->events, mask)) continue;
        if (it->node != nullptr && (!it->node->visible() || !it->node->enabled() || it->node->removal_pending() ||
                                    (it->node->parent() != nullptr && !it->node->parent()->accepts_input())))
            continue;

        route.blocker = *it;
        break;
    }

    Node* scope = route.blocker ? route.blocker->node : nullptr;
    if (route.blocker && scope == nullptr) return route;

    for (auto it = m_entries.rbegin(); it != m_entries.rend(); ++it) {
        ++m_checks;
        if (it->kind != EntryKind::Target || it->node == nullptr || !it->rect.contains(position) || !contains(it->events, mask) ||
            !it->node->accepts_input() || (scope != nullptr && !scope->contains(it->node)))
            continue;

        // parent geometry is finalized after its children. keep only the deepest hit node of each branch for bubbling.
        const auto related = std::find_if(route.targets.begin(), route.targets.end(), [&](const Entry& entry) {
            return entry.node->contains(it->node) || it->node->contains(entry.node);
        });
        if (related == route.targets.end()) {
            route.targets.push_back(*it);
        } else if (related->node != it->node && related->node->contains(it->node)) {
            *related = *it;
        }
    }

    return route;
}

std::size_t HitTestIndex::size() const {
    return m_entries.size();
}

std::size_t HitTestIndex::checks() const {
    return m_checks;
}
