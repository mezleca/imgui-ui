#pragma once

#include "widget.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

class UI;

namespace ui {
    class DropdownBodyNode;
    class DropdownTriggerNode;
    class TextWidget;

    enum class DropdownLabelPlacement {
        Above,
        Inline,
    };

    struct DropdownOption {
        std::string label;
        std::string value;
    };

    /// label, trigger and popup body are real child nodes for inspection and styling.
    class DropdownWidget final : public Widget {
    public:
        DropdownWidget(UI& ui, std::string& value, std::vector<DropdownOption> options, std::string id = {});

        DropdownWidget& set_label(std::string label);
        DropdownWidget& set_label_placement(DropdownLabelPlacement placement);
        DropdownWidget& set_placeholder(std::string placeholder);
        /// leaves the bound value unchanged when options are replaced.
        DropdownWidget& set_options(std::vector<DropdownOption> options);

        bool changed() const;
        bool is_open() const;
        TextWidget& label();
        Widget& trigger();
        Widget& body();
        bool on_draw() override;
        std::optional<std::string> content() const override;
        bool try_set_content(std::string content) override;

    private:
        friend class DropdownBodyNode;
        friend class DropdownTriggerNode;

        void configure_default_styles();
        bool draw_trigger(DropdownTriggerNode& trigger);
        bool draw_body(DropdownBodyNode& body);
        void draw_children() override;
        void on_layout() override;
        void on_draw_end() override;
        void draw_trigger_frame(Rect rect, std::string_view preview, bool open, const Style& style) const;
        const DropdownOption* selected_option() const;
        bool has_label() const;

        UI& m_ui;
        TextWidget* m_label_node = nullptr;
        DropdownTriggerNode* m_trigger = nullptr;
        DropdownBodyNode* m_body = nullptr;
        std::string* m_value;
        std::vector<DropdownOption> m_options;
        std::string m_placeholder = "select an option";
        DropdownLabelPlacement m_label_placement = DropdownLabelPlacement::Above;
        Rect m_trigger_rect{};
        bool m_open_requested = false;
        bool m_open = false;
        bool m_closing = false;
        bool m_changed = false;
    };
} // namespace ui
