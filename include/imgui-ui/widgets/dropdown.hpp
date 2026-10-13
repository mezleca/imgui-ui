#pragma once

#include "widget.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ui {
    class TextWidget;
    struct Theme;

    struct DropdownOption {
        std::string label;
        std::string value;

        bool operator==(const DropdownOption&) const = default;
    };

    class DropdownWidget : public Widget {
    public:
        explicit DropdownWidget(
            std::string& value, std::vector<DropdownOption> options = {}, std::string label = {},
            LabelPlacement placement = LabelPlacement::Above, std::string id = {}
        );

        DropdownWidget& set_label(std::string label, LabelPlacement placement = LabelPlacement::Above);
        DropdownWidget& set_placeholder(std::string placeholder);

        /// returns false if value is absent or already selected. a change closes the popup and notifies listeners.
        bool select_value(std::string_view value);

        /// replaces visible options without changing the bound value.
        DropdownWidget& set_options(std::vector<DropdownOption> options);
        void open();
        void close();

        bool is_open() const {
            return m_visibility == Visibility::Open;
        }

        TextWidget& label() {
            return *m_label_node;
        }

        Widget& trigger() const;

        Container& body() const;

    protected:
        void apply_theme_defaults(const Theme& theme) override;

    private:
        class Body;
        class Option;
        class Trigger;

        enum class Visibility : uint8_t {
            Closed,
            Open,
            Closing,
        };

        const DropdownOption* find_option(std::string_view option_value) const;
        const DropdownOption* selected_option() const;
        void finish_close();
        void update_label_layout();
        void event(UiEvent& event) override;

        std::string& m_value;
        std::vector<DropdownOption> m_options;
        std::string m_placeholder = "select an option";
        TextWidget* m_label_node = nullptr;
        Body* m_body = nullptr;
        Trigger* m_trigger = nullptr;
        Visibility m_visibility = Visibility::Closed;
        LabelPlacement m_label_placement = LabelPlacement::Above;
        ImVec2 m_label_spacing = {8.0F, 10.0F};
        ImVec2 m_arrow_size;
        float m_popup_gap = 0.0F;
        float m_transition_duration = 0.0F;
    };
} // namespace ui
