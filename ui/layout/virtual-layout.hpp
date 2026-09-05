#pragma once

#include "container.hpp"

#include <cstddef>
#include <functional>
#include <map>
#include <utility>

namespace ui {
    /// virtualizes drawing while the provider owns lazy creation and caching of row nodes.
    class VirtualLayout : public Container {
    public:
        using ItemProvider = std::function<Node&(size_t)>;

        explicit VirtualLayout(std::string id, float item_height);

        /// sets the data count and optionally replaces the callback that returns a direct child for each index.
        VirtualLayout& set_items(size_t count, ItemProvider provider = {});
        VirtualLayout& set_item_height(float height);
        VirtualLayout& set_spacing(float spacing);
        /// asks the provider for this many additional indices before and after the visible range.
        VirtualLayout& set_overscan(size_t count) {
            m_overscan = count;
            return *this;
        }

        /// adds manual pixels to a data index; indices refer to the source, and zero removes the offset.
        VirtualLayout& set_extra_offset(size_t index, float offset);
        float extra_offset(size_t index) const;
        VirtualLayout& clear_extra_offsets();

        size_t item_count() const {
            return m_item_count;
        }
        float item_height() const {
            return m_item_height;
        }
        float spacing() const {
            return m_spacing;
        }

    protected:
        void on_measure() override;
        bool paint() override;
        void draw_children() override;

    private:
        using ItemRange = std::pair<size_t, size_t>;

        float content_height() const;
        size_t item_boundary(float position, bool end) const;
        void draw_range(size_t first, size_t count, float height, float width, ItemRange buffer);

        float m_item_height = 1.0F;
        float m_spacing = 0.0F;
        size_t m_item_count = 0;
        size_t m_overscan = 0;
        ItemProvider m_item_provider;
        std::map<size_t, float> m_extra_offsets;
    };
} // namespace ui
