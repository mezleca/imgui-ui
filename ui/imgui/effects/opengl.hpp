#pragma once

#include "../../layout/geometry.hpp"

#include <glad/gl.h>
#include <imgui.h>

namespace ui {
    class OpenGlFullscreenEffect {
    public:
        OpenGlFullscreenEffect() = default;
        OpenGlFullscreenEffect(const OpenGlFullscreenEffect&) = delete;
        OpenGlFullscreenEffect& operator=(const OpenGlFullscreenEffect&) = delete;

        bool initialize(const char* fragment_shader);

        // shutdown must run before destroying the current opengl context.
        void shutdown();

        // begin intersects bounds with command.ClipRect before enabling the scissor test.
        bool begin(const ImDrawCmd& command, Rect bounds);
        GLint uniform(const char* name) const;

        // y uses the opengl framebuffer's bottom-left origin.
        Rect framebuffer_bounds() const;

        // copies framebuffer_bounds into captured_texture for a pass that reads prior draw commands.
        bool capture();
        GLuint captured_texture() const;
        void draw() const;

    private:
        GLuint m_program = 0;
        GLuint m_vertex_array = 0;
        GLuint m_texture = 0;
        ImVec2 m_texture_size{};
        Rect m_bounds;
    };
} // namespace ui
