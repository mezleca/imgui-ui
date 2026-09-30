#pragma once

#include "container.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <utility>

namespace ui {
    /// requests and draws source rows in its scroll viewport, including configured overscan.
    class VirtualLayout : public Container {
    public:
        /// returns the direct child that represents index. the child must remain alive through the current draw.
        using ItemProvider = std::function<Node&(size_t)>;

        /// enables vertical scrolling and uses item_height as the base height of every source row.
        explicit VirtualLayout(std::string id, float item_height);

        /// sets the source count. a supplied provider replaces the previous one, and a nonzero count requires a provider.
        VirtualLayout& set_items(size_t count, ItemProvider provider = {});
        /// changes the base row height and recalculates the scroll extent.
        VirtualLayout& set_item_height(float height);
        /// changes the gap between source rows and recalculates the scroll extent.
        VirtualLayout& set_spacing(float spacing) override;

        /// asks the provider for this many additional indices before and after the visible range.
        VirtualLayout& set_overscan(size_t count) {
            m_overscan = count;
            return *this;
        }

        /// adds manual pixels to a source index, and zero removes the offset.
        VirtualLayout& set_extra_offset(size_t index, float offset);
        float extra_offset(size_t index) const;
        VirtualLayout& clear_extra_offsets();

        size_t item_count() const {
            return m_item_count;
        }

    protected:
        void on_measure() override;
        void arrange_children() override {}
        void draw_children() override;
        bool paint() override;

    private:
        using ItemRange = std::pair<size_t, size_t>;

        float content_height() const;
        size_t item_boundary(float position, bool end) const;
        void draw_range(size_t first, size_t count, float height, float width, ItemRange buffer);

        float m_item_height = 1.0F;
        float m_spacing = 0.0F;
        float m_extra_height = 0.0F;
        size_t m_item_count = 0;
        size_t m_overscan = 0;
        ItemProvider m_item_provider;
        std::map<size_t, float> m_extra_offsets;
    };
} // namespace ui
