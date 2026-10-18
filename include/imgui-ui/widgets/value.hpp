#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace ui {
    /// stores a widget value either by reference or in its own storage.
    /// binding an lvalue keeps a non-owning pointer, so the lvalue must outlive the widget.
    /// callbacks run only for changes made through set() or edit(), not for writes through the bound lvalue.
    template <typename T>
    class WidgetValue {
    public:
        using ChangeCallback = std::function<void(const T&)>;

        WidgetValue() : m_owned(std::in_place) {}
        WidgetValue(T& value) : m_ref(std::addressof(value)) {}
        WidgetValue(T&& value) : m_owned(std::move(value)) {}
        WidgetValue(const T& value) : m_owned(value) {}

        WidgetValue(const WidgetValue&) = default;
        WidgetValue(WidgetValue&&) = default;
        WidgetValue& operator=(const WidgetValue&) = delete;
        WidgetValue& operator=(WidgetValue&&) = delete;

        [[nodiscard]] const T& value() const {
            return m_owned ? *m_owned : *m_ref;
        }

        [[nodiscard]] bool bound() const {
            return m_ref != nullptr;
        }

        bool set(T value) {
            if (this->value() == value) {
                return false;
            }

            target() = std::move(value);
            notify();
            return true;
        }

        template <std::predicate<T&> F>
        bool edit(F&& mutate) {
            // mutate returns whether it changed the value
            if (!std::invoke(std::forward<F>(mutate), target())) {
                return false;
            }

            notify();
            return true;
        }

        WidgetValue& on_change(ChangeCallback callback) {
            m_callback = std::move(callback);
            return *this;
        }

    private:
        T& target() {
            return m_owned ? *m_owned : *m_ref;
        }

        void notify() {
            if (m_callback) {
                m_callback(value());
            }
        }

        std::optional<T> m_owned;
        T* m_ref = nullptr;
        ChangeCallback m_callback;
    };
} // namespace ui
