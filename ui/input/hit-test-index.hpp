#pragma once

#include "../layout/geometry.hpp"
#include "event.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <vector>

namespace ui {
    class Node;

    using InputCallback = std::function<void(UiEvent&)>;

    /// stores frame-local hit regions in paint order for InputRouter target and blocker queries.
    class HitTestIndex {
    public:
        static constexpr uint32_t no_callback = std::numeric_limits<uint32_t>::max();

        enum class EntryKind : uint8_t {
            Target,
            Blocker,
        };

        struct Entry {
            Node* node = nullptr;
            Rect rect;
            EventMask events = EventMask::Pointer;
            uint32_t callback = no_callback;
            EntryKind kind = EntryKind::Target;
        };

        void begin_frame(std::size_t expected_entries);
        void add(Node* node, EntryKind kind, Rect rect, EventMask events, InputCallback callback);
        void register_node(Node& node, bool blocker, Rect input_rect, Rect visual_rect);
        void erase(Node& node);
        void erase_subtree(Node& subtree);

        const Entry* resolve(ImVec2 position, EventType type, const Entry*& blocker) const;
        const Entry* target_at(ImVec2 position, EventType type = EventType::PointerMove, const Node* scope = nullptr) const;

        bool invoke_callback(const Entry& entry, UiEvent& event) const;

        std::size_t size() const;
        std::size_t checks() const;

    private:
        const Entry* blocking_entry_at(ImVec2 position, EventType type, const Node* target) const;

        std::vector<Entry> m_entries;
        std::deque<InputCallback> m_callbacks;
        bool m_has_blockers = false;
        mutable std::size_t m_checks = 0;
    };

} // namespace ui
