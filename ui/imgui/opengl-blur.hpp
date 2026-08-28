#pragma once

namespace ui {
    [[nodiscard]] bool initialize_opengl_blur();
    void begin_opengl_blur_frame();
    void shutdown_opengl_blur();
} // namespace ui
