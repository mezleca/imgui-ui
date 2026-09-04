#pragma once

#include <ui/runtime.hpp>

#include <string>

class UI;

// prepares shared assets before either example constructs its runtime.
void configure_demo_runtime(ui::RuntimeConfig& config);

// attaches the retained demo tree after the surface owns an active imgui context.
void setup_demo(UI& surface, std::string backend);
