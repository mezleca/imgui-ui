#pragma once

#include "../string-hash.hpp"
#include "values.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace ui {
    /// stores key-value while preserving their concrete type for debugger controls.
    class StyleVariableStore {
    public:
        void set(std::string_view key, StyleValue value);

        template <typename T>
        void set(std::string_view key, T value) {
            set(key, StyleValue{std::move(value)});
        }

        template <typename T>
        T* get(std::string_view key) {
            StyleValue* value = find(key);
            return value == nullptr ? nullptr : std::get_if<T>(value);
        }

        template <typename T>
        const T* get(std::string_view key) const {
            const StyleValue* value = find(key);
            return value == nullptr ? nullptr : std::get_if<T>(value);
        }

        StyleValue* find(std::string_view key);

        const StyleValue* find(std::string_view key) const;

        bool is_transitioning() const;

        auto begin() {
            return m_vars.begin();
        }

        auto end() {
            return m_vars.end();
        }

        auto begin() const {
            return m_vars.begin();
        }

        auto end() const {
            return m_vars.end();
        }

    private:
        std::unordered_map<std::string, StyleValue, StringHash, std::equal_to<>> m_vars;
    };

} // namespace ui
