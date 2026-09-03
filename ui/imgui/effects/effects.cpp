#include "effects.hpp"

#include <algorithm>

ui::EffectRegistry::Entry* ui::EffectRegistry::find(EffectId id) {
    const auto iterator = std::find_if(m_entries.begin(), m_entries.end(), [id](const Entry& entry) { return entry.id == id; });
    return iterator == m_entries.end() ? nullptr : &*iterator;
}

const ui::EffectRegistry::Entry* ui::EffectRegistry::find(EffectId id) const {
    const auto iterator = std::find_if(m_entries.begin(), m_entries.end(), [id](const Entry& entry) { return entry.id == id; });
    return iterator == m_entries.end() ? nullptr : &*iterator;
}

ui::EffectId ui::EffectRegistry::register_effect(EffectDefinition definition) {
    if (definition.render == nullptr) {
        return 0;
    }

    if (m_initialized && definition.initialize != nullptr && !definition.initialize(definition.user_data)) {
        return 0;
    }

    if (m_next_id == 0) {
        m_next_id = 1;
    }

    const EffectId id = m_next_id++;
    m_entries.push_back({id, definition, m_initialized});
    return id;
}

bool ui::EffectRegistry::unregister_effect(EffectId id) {
    const auto iterator = std::find_if(m_entries.begin(), m_entries.end(), [id](const Entry& entry) { return entry.id == id; });
    if (iterator == m_entries.end()) {
        return false;
    }

    if (iterator->initialized && iterator->definition.shutdown != nullptr) {
        iterator->definition.shutdown(iterator->definition.user_data);
    }
    m_entries.erase(iterator);
    return true;
}

bool ui::EffectRegistry::initialize() {
    if (m_initialized) {
        return true;
    }

    for (Entry& entry : m_entries) {
        if (entry.definition.initialize != nullptr && !entry.definition.initialize(entry.definition.user_data)) {
            for (Entry& initialized_entry : m_entries) {
                if (!initialized_entry.initialized) {
                    break;
                }

                if (initialized_entry.definition.shutdown != nullptr) {
                    initialized_entry.definition.shutdown(initialized_entry.definition.user_data);
                }
                initialized_entry.initialized = false;
            }
            return false;
        }

        entry.initialized = true;
    }

    m_initialized = true;
    return true;
}

void ui::EffectRegistry::begin_frame() {
    if (!m_initialized) {
        return;
    }

    for (Entry& entry : m_entries) {
        if (entry.initialized && entry.definition.begin_frame != nullptr) {
            entry.definition.begin_frame(entry.definition.user_data);
        }
    }
}

void ui::EffectRegistry::shutdown() {
    for (auto iterator = m_entries.rbegin(); iterator != m_entries.rend(); ++iterator) {
        if (!iterator->initialized) {
            continue;
        }

        if (iterator->definition.shutdown != nullptr) {
            iterator->definition.shutdown(iterator->definition.user_data);
        }
        iterator->initialized = false;
    }

    m_initialized = false;
}

bool ui::EffectRegistry::submit(ImDrawList& draw_list, EffectId id, void* payload) const {
    const Entry* entry = find(id);
    if (!m_initialized || entry == nullptr || !entry->initialized || entry->definition.render == nullptr) {
        return false;
    }

    draw_list.AddCallback(entry->definition.render, payload);
    draw_list.AddCallback(ImDrawCallback_ResetRenderState, nullptr);
    return true;
}
