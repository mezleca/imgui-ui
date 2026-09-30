#pragma once

#include <imgui-ui/runtime.hpp>

#include <string>

namespace ui {
    class Surface;
}

// fills the runtime config before runtime takes ownership of its assets.
void configure_demo_runtime(ui::RuntimeConfig& config);

// attaches the demo tree after ui creates its imgui context.
void setup_demo(ui::Surface& surface, std::string backend);
