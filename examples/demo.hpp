#pragma once

#include <ui/runtime.hpp>

#include <string>

class UI;

// fills the runtime config before runtime takes ownership of its assets.
void configure_demo_runtime(ui::RuntimeConfig& config);

// attaches the demo tree after ui creates its imgui context.
void setup_demo(UI& surface, std::string backend);
