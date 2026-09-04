#pragma once

#include "widget.hpp"

#include <string>
#include <string_view>
#include <vector>

class UI;

namespace ui {
    class DropdownBodyNode;
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

        bool is_open() const {
            return m_state.is_open();
        }

        TextWidget& label() {
            return *m_label_node;
        }

        Widget& trigger();
        Widget& body();

    protected:
        void apply_theme_defaults(const Theme& theme) override;

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

            void open();

            void close();

            void finish_close() {
                visibility = Visibility::Closed;
            }

            enum class Visibility {
                Closed,
                Open,
                Closing,
            };

            std::string* value = nullptr;
            std::vector<DropdownOption> options;
            std::string placeholder = "select an option";
            DropdownBodyNode* body = nullptr;
            Visibility visibility = Visibility::Closed;
        };

        friend class DropdownBodyNode;
        friend class DropdownTriggerNode;

        void draw_children() override;
        void on_measure() override;
        void on_layout() override;
        bool has_label() const;

        TextWidget* m_label_node = nullptr;
        DropdownTriggerNode* m_trigger = nullptr;
        DropdownBodyNode* m_body = nullptr;
        State m_state;
    };
} // namespace ui
