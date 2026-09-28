#include "variables.hpp"

using namespace ui;

void StyleVariableStore::set(std::string_view key, StyleValue value) {
    auto existing_it = m_vars.find(key);
    if (existing_it != m_vars.end()) {
        existing_it->second = std::move(value);
        return;
    }
    m_vars.emplace(key, std::move(value));
}

StyleValue* StyleVariableStore::find(std::string_view key) {
    auto value_it = m_vars.find(key);
    return value_it == m_vars.end() ? nullptr : &value_it->second;
}

const StyleValue* StyleVariableStore::find(std::string_view key) const {
    auto value_it = m_vars.find(key);
    return value_it == m_vars.end() ? nullptr : &value_it->second;
}

bool StyleVariableStore::is_transitioning() const {
    for (const auto& entry : m_vars) {
        if (std::visit([](const auto& item) { return item.is_transitioning(); }, entry.second)) return true;
    }
    return false;
}
