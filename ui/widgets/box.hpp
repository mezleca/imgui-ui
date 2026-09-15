#pragma once

#include "../layout/container.hpp"

#include <string>
#include <utility>

namespace ui {
    class BoxWidget : public Container {
    public:
        explicit BoxWidget(std::string id = {}, LayoutSize size = {fit(), fit()}) : Container(std::move(id), "Box") {
            set_size(size);
        }
    };
} // namespace ui
