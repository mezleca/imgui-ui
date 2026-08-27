#pragma once

#include "../style/styled-node.hpp"
#include "text-value.hpp"

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

namespace ui {
    class TextWidget : public StyledNode {
    public:
        explicit TextWidget(std::string text) : StyledNode({}, "Text"), m_text(std::make_unique<GenericValue>(std::move(text))) {}

        template <typename... Args>
        TextWidget(std::string format, std::tuple<Args...> values)
            : StyledNode({}, "Text"), m_text(std::make_unique<TextFormatted<Args...>>(std::move(format))) {
            static_cast<TextFormatted<Args...>*>(m_text.get())->set(std::move(values));
        }

        /// a negative wrap value disables wrapping.
        void set_wrap(float wrap);
        bool on_draw() override;
        bool try_set_content(std::string content) override;

        std::optional<std::string> content() const override;

    private:
        void on_measure() override;

        std::unique_ptr<GenericValue> m_text;
        float m_wrap = -1.0F;
    };

} // namespace ui
