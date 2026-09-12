#include "hit-test-index.hpp"

#include "../tree/node.hpp"

#include <imgui.h>

#include <algorithm>
#include <utility>

using namespace ui;

void HitTestIndex::begin_frame(std::size_t expected_entries) {
    m_entries.clear();
    m_callbacks.clear();
    m_has_blockers = false;
    m_checks = 0;

    if (m_entries.capacity() < expected_entries) {
        m_entries.reserve(expected_entries);
    }
}

void HitTestIndex::add(Node* node, EntryKind kind, Rect rect, EventMask events, InputCallback callback) {
    m_has_blockers |= kind == EntryKind::Blocker;

    const uint32_t callback_index = callback ? static_cast<uint32_t>(m_callbacks.size()) : no_callback;
    if (callback) {
        m_callbacks.push_back(std::move(callback));
    }

    m_entries.push_back(Entry{node, rect, events, callback_index, kind});
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

    // clipped pixels cannot receive input from this window.
    if (ImGui::GetCurrentContext() != nullptr) {
        const ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const ImVec2 clip_min = draw_list->GetClipRectMin();
        const ImVec2 clip_max = draw_list->GetClipRectMax();
        input_rect.min.x = std::max(input_rect.min.x, clip_min.x);
        input_rect.min.y = std::max(input_rect.min.y, clip_min.y);
        input_rect.max.x = std::min(input_rect.max.x, clip_max.x);
        input_rect.max.y = std::min(input_rect.max.y, clip_max.y);
    }

    if (!input_rect.valid()) {
        return;
    }

    const EntryKind kind = blocker ? EntryKind::Blocker : EntryKind::Target;
    add(&node, kind, input_rect, EventMask::Pointer, {});
}

void HitTestIndex::erase(Node& node) {
    std::erase_if(m_entries, [&node](const Entry& entry) { return entry.node == &node; });
}

void HitTestIndex::erase_subtree(Node& subtree) {
    std::erase_if(m_entries, [&subtree](const Entry& entry) { return subtree.contains(entry.node); });
}

const HitTestIndex::Entry* HitTestIndex::resolve(ImVec2 position, EventType type, const Entry*& blocker) const {
    const Entry* target = target_at(position, type);
    blocker = m_has_blockers ? blocking_entry_at(position, type, target == nullptr ? nullptr : target->node) : nullptr;

    if (blocker != nullptr && blocker->node != nullptr) {
        target = target_at(position, type, blocker->node);
        blocker = blocking_entry_at(position, type, target == nullptr ? nullptr : target->node);
    }

    return target;
}

const HitTestIndex::Entry* HitTestIndex::target_at(ImVec2 position, EventType type, const Node* scope) const {
    const Entry* target = nullptr;
    for (auto it = m_entries.rbegin(); it != m_entries.rend(); ++it) {
        ++m_checks;

        if (it->kind != EntryKind::Target || it->node == nullptr || !it->rect.contains(position) ||
            !contains(it->events, event_mask(type)) || (scope != nullptr && !scope->contains(it->node))) {
            continue;
        }

        if (!it->node->visible() || !it->node->accepts_input()) {
            continue;
        }

        if (target == nullptr || target->node->contains(it->node)) {
            target = &*it;
        }
    }

    return target;
}

const HitTestIndex::Entry* HitTestIndex::blocking_entry_at(ImVec2 position, EventType type, const Node* target) const {
    const EventMask mask = event_mask(type);
    if (mask == EventMask::None) {
        return nullptr;
    }

    for (auto it = m_entries.rbegin(); it != m_entries.rend(); ++it) {
        ++m_checks;
        if (it->kind == EntryKind::Blocker && it->rect.contains(position) && contains(it->events, mask) &&
            (it->node == nullptr || (it->node->visible() && it->node->enabled() && !it->node->contains(target)))) {
            return &*it;
        }
    }

    return nullptr;
}

bool HitTestIndex::invoke_callback(const Entry& entry, UiEvent& event) const {
    if (entry.callback == no_callback) {
        return false;
    }

    m_callbacks[entry.callback](event);
    return true;
}

std::size_t HitTestIndex::size() const {
    return m_entries.size();
}

std::size_t HitTestIndex::checks() const {
    return m_checks;
}
