#include "opengl.hpp"

#include <algorithm>
#include <cmath>

using namespace ui;

static constexpr const char* VERTEX_SHADER = R"(#version 330 core
void main() {
const vec2 positions[3] = vec2[](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
})";

static GLuint compile_shader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) {
        return shader;
    }

    glDeleteShader(shader);
    return 0;
}

bool OpenGlFullscreenEffect::initialize(const char* fragment_shader) {
    const GLuint vertex = compile_shader(GL_VERTEX_SHADER, VERTEX_SHADER);
    const GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_shader);
    if (vertex == 0 || fragment == 0) {
        if (vertex != 0) glDeleteShader(vertex);
        if (fragment != 0) glDeleteShader(fragment);
        return false;
    }

    m_program = glCreateProgram();
    glAttachShader(m_program, vertex);
    glAttachShader(m_program, fragment);
    glLinkProgram(m_program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint linked = GL_FALSE;
    glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        glDeleteProgram(m_program);
        m_program = 0;
        return false;
    }

    glGenVertexArrays(1, &m_vertex_array);
    return true;
}

void OpenGlFullscreenEffect::shutdown() {
    if (m_program != 0) glDeleteProgram(m_program);
    if (m_vertex_array != 0) glDeleteVertexArrays(1, &m_vertex_array);
    if (m_texture != 0) glDeleteTextures(1, &m_texture);
    m_program = 0;
    m_vertex_array = 0;
    m_texture = 0;
    m_texture_size = {};
}

bool OpenGlFullscreenEffect::begin(const ImDrawCmd& command, Rect bounds) {
    if (m_program == 0 || m_vertex_array == 0 || !bounds.valid()) {
        return false;
    }

    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const int width = viewport[2];
    const int height = viewport[3];
    if (width <= 0 || height <= 0) {
        return false;
    }

    const ImDrawData* draw_data = ImGui::GetDrawData();
    const ImVec2 display_position = draw_data == nullptr ? ImVec2{} : draw_data->DisplayPos;
    const ImVec2 scale = draw_data == nullptr ? ImVec2{1.0F, 1.0F} : draw_data->FramebufferScale;
    const auto to_framebuffer = [&](ImVec2 point) {
        return ImVec2{(point.x - display_position.x) * scale.x, (point.y - display_position.y) * scale.y};
    };

    const ImVec2 bounds_min = to_framebuffer(bounds.min);
    const ImVec2 bounds_max = to_framebuffer(bounds.max);
    const ImVec2 clip_min = to_framebuffer({command.ClipRect.x, command.ClipRect.y});
    const ImVec2 clip_max = to_framebuffer({command.ClipRect.z, command.ClipRect.w});
    const int left = std::max(
        std::clamp(static_cast<int>(std::floor(bounds_min.x)), 0, width),
        std::clamp(static_cast<int>(std::floor(clip_min.x)), 0, width)
    );
    const int right = std::min(
        std::clamp(static_cast<int>(std::ceil(bounds_max.x)), 0, width),
        std::clamp(static_cast<int>(std::ceil(clip_max.x)), 0, width)
    );
    const int top = std::max(
        std::clamp(static_cast<int>(std::floor(bounds_min.y)), 0, height),
        std::clamp(static_cast<int>(std::floor(clip_min.y)), 0, height)
    );
    const int bottom = std::min(
        std::clamp(static_cast<int>(std::ceil(bounds_max.y)), 0, height),
        std::clamp(static_cast<int>(std::ceil(clip_max.y)), 0, height)
    );
    if (right <= left || bottom <= top) {
        return false;
    }

    m_bounds = {{bounds_min.x, height - bounds_max.y}, {bounds_max.x, height - bounds_min.y}};
    glEnable(GL_SCISSOR_TEST);
    glScissor(left, height - bottom, right - left, bottom - top);
    glDisable(GL_BLEND);
    glUseProgram(m_program);
    glBindVertexArray(m_vertex_array);
    return true;
}

GLint OpenGlFullscreenEffect::uniform(const char* name) const {
    return glGetUniformLocation(m_program, name);
}

Rect OpenGlFullscreenEffect::framebuffer_bounds() const {
    return m_bounds;
}

bool OpenGlFullscreenEffect::capture() {
    const int left = static_cast<int>(std::floor(m_bounds.min.x));
    const int bottom = static_cast<int>(std::floor(m_bounds.min.y));
    const int right = static_cast<int>(std::ceil(m_bounds.max.x));
    const int top = static_cast<int>(std::ceil(m_bounds.max.y));
    const int width = right - left;
    const int height = top - bottom;
    if (width <= 0 || height <= 0) {
        return false;
    }

    if (m_texture == 0) {
        glGenTextures(1, &m_texture);
    }

    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (m_texture_size.x != width || m_texture_size.y != height) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        m_texture_size = {static_cast<float>(width), static_cast<float>(height)};
    }

    m_bounds = {{static_cast<float>(left), static_cast<float>(bottom)}, {static_cast<float>(right), static_cast<float>(top)}};
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, left, bottom, width, height);
    return true;
}

GLuint OpenGlFullscreenEffect::captured_texture() const {
    return m_texture;
}

void OpenGlFullscreenEffect::draw() const {
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisable(GL_SCISSOR_TEST);
}
