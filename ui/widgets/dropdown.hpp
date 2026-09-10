#pragma once

#include "widget.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ui {
    class UI;
    class DropdownBodyNode;
    class DropdownOptionNode;
    class DropdownTriggerNode;
    class TextWidget;
    struct Theme;

    struct DropdownOption {
        std::string label;
        std::string value;

        bool operator==(const DropdownOption&) const = default;
    };

    /// exposes the label, trigger and popup body for styling and inspection.
    class DropdownWidget final : public Widget {
    public:
        DropdownWidget(UI& ui, std::string& value, std::vector<DropdownOption> options, std::string id = {});

        DropdownWidget& set_label(std::string label);
        DropdownWidget& set_placeholder(std::string placeholder);
        bool select_value(std::string_view value);
        /// replaces visible options without changing the bound value.
        DropdownWidget& set_options(std::vector<DropdownOption> options);
        void open();
        void close();

        /// returns true while the popup accepts option input.
        bool is_open() const {
            return m_state.is_open();
        }

        TextWidget& label() {
            return *m_label_node;
        }

        /// returns the custom-painted trigger node.
        Widget& trigger();

        /// returns the popup body node used for layout and inspection.
        Widget& body();

    protected:
        void apply_theme_defaults(const Theme& theme) override;
        bool paint() override;

    private:
        struct State {
            bool is_open() const {
                return visibility == Visibility::Open;
            }

            bool is_closing() const {
                return visibility == Visibility::Closing;
            }

            bool is_closed() const {
                return visibility == Visibility::Closed;
            }

            // trigger and option rows use these shared selection operations.
            const DropdownOption* find_option(std::string_view option_value) const;
            const DropdownOption* selected_option() const;
            bool select(std::size_t index);

            void open();

            void close();

            void finish_close();

            enum class Visibility : uint8_t {
                Closed,
                Open,
                Closing,
            };

            std::string* value = nullptr;
            std::vector<DropdownOption> options;
            std::string placeholder = "select an option";
            DropdownWidget* owner = nullptr;
            DropdownBodyNode* body = nullptr;
            DropdownTriggerNode* trigger = nullptr;
            Visibility visibility = Visibility::Closed;
            ImVec2 arrow_size{};
            float popup_gap = 0.0F;
            float transition_duration = 0.0F;
        };

        friend class DropdownBodyNode;
        friend class DropdownOptionNode;
        friend class DropdownTriggerNode;

        void draw_children() override;
        void on_measure() override;
        void on_layout() override;
        void on_event(UiEvent& event) override;
        bool has_label() const;

        TextWidget* m_label_node = nullptr;
        DropdownTriggerNode* m_trigger = nullptr;
        DropdownBodyNode* m_body = nullptr;
        State m_state;
    };
} // namespace ui
