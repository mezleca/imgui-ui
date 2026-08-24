#pragma once

#include "../../resources/icon.hpp"

#include <memory>

namespace ui {
    std::unique_ptr<IconLoader> make_sdl_icon_loader();
} // namespace ui
