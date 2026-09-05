#include "virtual-layout.hpp"

#include <algorithm>
#include <climits>
#include <cmath>
#include <stdexcept>
#include <utility>

using namespace ui;

VirtualLayout::VirtualLayout(std::string id, float item_height) : Container(std::move(id), "VirtualLayout") {
    set_item_height(item_height);
    set_scrollable(true);
}

VirtualLayout& VirtualLayout::set_items(size_t count, ItemProvider provider) {
    if (count >= INT_MAX) {
        throw std::length_error("virtual item count exceeds ImGuiListClipper capacity");
    }

    if (count != 0 && !provider && !m_item_provider) {
        throw std::invalid_argument("virtual items require a provider");
    }

    // replace the callback only when one is supplied, so callers can resize a source without losing its cache.
    if (provider) {
        m_item_provider = std::move(provider);
    } else if (m_item_count == count) {
        return *this;
    }

    // discard offsets that no longer point at a source item before invalidating fit-height measurements.
    m_item_count = count;
    m_extra_offsets.erase(m_extra_offsets.lower_bound(count), m_extra_offsets.end());

    invalidate_measure();
    return *this;
}

VirtualLayout& VirtualLayout::set_item_height(float height) {
    if (!std::isfinite(height) || height <= 0.0F) {
        throw std::invalid_argument("virtual item height must be finite and positive");
    }
    if (m_item_height == height) return *this;

    // changing the base row height changes every row position and the total scroll extent.
    m_item_height = height;
    invalidate_measure();
    return *this;
}

VirtualLayout& VirtualLayout::set_spacing(float spacing) {
    if (!std::isfinite(spacing) || spacing < 0.0F) {
        throw std::invalid_argument("virtual spacing must be finite and nonnegative");
    }

    if (m_spacing == spacing) return *this;

    // changing spacing changes every row position and the total scroll extent.
    m_spacing = spacing;
    invalidate_measure();
    return *this;
}

VirtualLayout& VirtualLayout::set_extra_offset(size_t index, float offset) {
    if (index >= m_item_count) {
        throw std::out_of_range("virtual item index is out of range");
    }

    if (!std::isfinite(offset) || offset < 0.0F) {
        throw std::invalid_argument("virtual extra offset must be finite and nonnegative");
    }

    // zero removes the map entry so only expanded rows split the uniform clipper runs.
    if (offset == 0.0F) {
        m_extra_offsets.erase(index);
    } else {
        m_extra_offsets[index] = offset;
    }

    invalidate_measure();
    return *this;
}

float VirtualLayout::extra_offset(size_t index) const {
    const auto found = m_extra_offsets.find(index);
    return found != m_extra_offsets.end() ? found->second : 0.0F;
}

VirtualLayout& VirtualLayout::clear_extra_offsets() {
    m_extra_offsets.clear();
    invalidate_measure();
    return *this;
}

float VirtualLayout::content_height() const {
    const size_t count = m_item_count;
    float height = static_cast<float>(count) * m_item_height;
    if (count > 1) height += static_cast<float>(count - 1) * m_spacing;

    for (const auto& entry : m_extra_offsets) {
        height += entry.second;
    }

    return height;
}

void VirtualLayout::on_measure() {
    const LayoutSize& size = layout().size_spec();

    if (size.width.mode == LayoutSizeMode::Fit) {
        throw std::invalid_argument("virtual layout width must be fixed or growing");
    }

    // fit height is computable from the source count and offsets without creating any nodes.
    if (size.height.mode == LayoutSizeMode::Fit) {
        set_measured_size({0.0F, content_height() + style().padding().y * 2.0F}, false, true);
    }
}

bool VirtualLayout::paint() {
    // reserve the full logical height so the child window exposes scrolling even when most rows are clipped.
    ImGui::SetNextWindowContentSize({0.0F, content_height()});
    return Container::paint();
}

void VirtualLayout::draw_children() {
    const float width = std::max(0.0F, ImGui::GetContentRegionAvail().x);
    ItemRange buffer{};

    // map the viewport to source indices once
    // each run below intersects this interval before asking the provider.
    if (m_overscan != 0 && m_item_count != 0) {
        const ImDrawList* draw_list = ImGui::GetWindowDrawList();

        const float origin = ImGui::GetCursorScreenPos().y;
        const float top = draw_list->GetClipRectMin().y - origin;
        const float bottom = draw_list->GetClipRectMax().y - origin;

        if (bottom > 0.0F && top < content_height()) {
            const size_t first = item_boundary(top, false);
            const size_t last = item_boundary(bottom, true);

            // extend both ends by the requested number of source rows without underflowing or exceeding the count.
            buffer.first = first > m_overscan ? first - m_overscan : 0;
            buffer.second = last + std::min(m_overscan, m_item_count - last);
        }
    }

    size_t first = 0;

    // each expanded row becomes its own run; all rows between offsets share one height for ImGuiListClipper.
    for (const auto& [index, extra] : m_extra_offsets) {
        draw_range(first, index - first, m_item_height, width, buffer);
        draw_range(index, 1, m_item_height + extra, width, buffer);
        first = index + 1;
    }

    draw_range(first, m_item_count - first, m_item_height, width, buffer);
}

size_t VirtualLayout::item_boundary(float position, bool end) const {
    // convert a content-space y position to a source index while removing expansion pixels already crossed.
    const float stride = m_item_height + m_spacing;
    for (const auto& [index, extra] : m_extra_offsets) {
        const float start = static_cast<float>(index) * stride;
        if (position < start) break;
        if (position < start + stride + extra) {
            return index + (end ? position > start : position >= start + m_item_height + extra);
        }
        position -= extra;
    }

    const float index = end ? std::ceil(position / stride) : std::floor((position + m_spacing) / stride);
    return std::min(m_item_count, static_cast<size_t>(std::max(0.0F, index)));
}

void VirtualLayout::draw_range(size_t first, size_t count, float height, float width, ItemRange buffer) {
    if (count == 0) return;

    ImGuiListClipper clipper;

    const float stride = height + m_spacing;
    const float offset = ImGui::GetCursorPosY() - ImGui::GetCursorStartPos().y - ImGui::GetScrollY();
    const ImVec2 start = ImGui::GetCursorScreenPos();

    clipper.Begin(static_cast<int>(count), stride);

    const size_t begin = std::max(first, buffer.first);
    const size_t end = std::min(first + count, buffer.second);

    if (begin < end) {
        clipper.IncludeItemsByIndex(static_cast<int>(begin - first), static_cast<int>(end - first));
    }

    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const size_t index = first + static_cast<size_t>(row);
            const float screen_y = start.y + static_cast<float>(row) * stride;

            // the clipper may return a boundary row
            // avoid creating it unless it is visible or inside overscan.
            if (!(index >= begin && index < end) &&
                !ImGui::IsRectVisible({start.x, screen_y}, {start.x + width, screen_y + height})) {
                continue;
            }

            // get child from provider.
            Node& child = m_item_provider(index);

            if (!child.visible()) {
                continue;
            }

            // arrange the child at its logical y position, then draw only this selected row.
            const float y = offset + static_cast<float>(row) * stride;
            arrange_child(child, {width, height}, {.offset = {0.0F, y}});
            child.draw();
        }
    }
}
