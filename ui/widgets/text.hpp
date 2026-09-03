#pragma once

#include "../style/styled-node.hpp"
#include "text-value.hpp"

#include <format>
#include <string>
#include <tuple>
#include <utility>

namespace ui {
    enum class TextOverflow {
        Clip,
        Ellipsis,
    };

    class TextWidget : public StyledNode {
    public:
        explicit TextWidget(std::string text) : StyledNode({}, "Text"), m_text(std::move(text)) {}

        template <typename... Args>
        TextWidget(std::string format, std::tuple<Args...> values)
            : StyledNode({}, "Text"),
              m_text(
                  std::apply(
                      [&format](const auto&... args) { return std::vformat(format, std::make_format_args(args...)); }, values
                  )
              ) {}

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

    private:
        bool paint() override;
        void on_measure() override;

        GenericValue m_text;
        float m_wrap = -1.0F;
        TextOverflow m_overflow = TextOverflow::Clip;
    };

} // namespace ui
