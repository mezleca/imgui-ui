#pragma once

#include "../style/styled-node.hpp"
#include "text-value.hpp"

#include <cstdint>
#include <format>
#include <string>
#include <utility>

namespace ui {
    enum class TextOverflow : uint8_t {
        /// clips text at the content bounds.
        Clip,
        /// replaces clipped text with an ellipsis.
        Ellipsis,
    };

    class TextWidget : public StyledNode {
    public:
        explicit TextWidget(std::string text) : StyledNode({}, "Text"), m_text(std::move(text)) {}

        template <typename... Args>
            requires(sizeof...(Args) > 0)
        TextWidget(std::string format, Args&&... args)
            : StyledNode({}, "Text"), m_text(std::vformat(format, std::make_format_args(args...))) {}

        TextWidget& set_wrap(float width);
        TextWidget& set_overflow(TextOverflow overflow);
        TextOverflow overflow() const {
            return m_overflow;
        }
        bool empty() const;
        TextWidget& set_size(LayoutSize size) {
            StyledNode::set_size(size);
            return *this;
        }
        TextWidget& set_text(std::string text);

    protected:
        void apply_theme_defaults(const Theme& theme) override;

    private:
        bool paint() override;
        void on_measure() override;

        GenericValue m_text;
        float m_wrap = -1.0F;
        TextOverflow m_overflow = TextOverflow::Clip;
    };

} // namespace ui
