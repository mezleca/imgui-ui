#include "runtime.hpp"

#include <utility>

namespace ui {
    Runtime::Runtime(RuntimeConfig config)
        : m_theme(std::move(config.theme)), m_assets(std::move(config.icon_loader)),
          m_performance_directory(std::move(config.performance_directory)) {}

    void Runtime::release_context(ImGuiContext* context) {
        m_assets.release_context(context);
    }
} // namespace ui
