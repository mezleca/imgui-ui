#include "opengl.hpp"

#include "shadow.hpp"
#include "../effects.hpp"

#include <glad/gl.h>

#include <algorithm>
#include <cmath>
#include <unordered_map>

struct BoxShadowGlState {
    GLuint vertex_array = 0;
    GLuint program = 0;
    GLint shape = -1;
    GLint rounding = -1;
    GLint sigma = -1;
    GLint viewport_height = -1;
    GLint color = -1;
};

static std::unordered_map<ImGuiContext*, BoxShadowGlState> gl_states;
static BoxShadowGlState* gl_state = nullptr;

static bool select_gl_state() {
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr) {
        gl_state = nullptr;
        return false;
    }

    gl_state = &gl_states[context];
    return true;
}

static constexpr const char* VERTEX_SHADER = R"(#version 330 core
void main() {
const vec2 positions[3] = vec2[](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
})";

static constexpr const char* FRAGMENT_SHADER = R"(#version 330 core
uniform vec4 shape;
uniform float rounding;
uniform float sigma;
uniform float viewport_height;
uniform vec4 shadow_color;
out vec4 color;

// adapted from evan wallace's cc0 rounded box shadow shader.
float erf_approx(float value) {
float direction = value < 0.0 ? -1.0 : 1.0;
float absolute = abs(value);
float polynomial = 1.0 + (0.278393 + (0.230389 + 0.078108 * absolute * absolute) * absolute) * absolute;
polynomial *= polynomial;
return direction - direction / (polynomial * polynomial);
}

float gaussian(float value, float width) {
return exp(-(value * value) / (2.0 * width * width)) / (sqrt(2.0 * 3.141592653589793) * width);
}

float rounded_shadow_x(float x, float y, float width, float corner, vec2 half_size) {
float delta = min(half_size.y - corner - abs(y), 0.0);
float curved = half_size.x - corner + sqrt(max(0.0, corner * corner - delta * delta));
float scale = sqrt(0.5) / width;
vec2 integral = 0.5 + 0.5 * vec2(erf_approx((x - curved) * scale), erf_approx((x + curved) * scale));
return integral.y - integral.x;
}

float rounded_shadow(vec2 point, vec2 half_size, float width, float corner) {
float low = point.y - half_size.y;
float high = point.y + half_size.y;
float start = clamp(-3.0 * width, low, high);
float end = clamp(3.0 * width, low, high);
float step = (end - start) / 4.0;
float value = 0.0;
for (int index = 0; index < 4; ++index) {
    float sample = start + step * (float(index) + 0.5);
    value += rounded_shadow_x(point.x, point.y - sample, width, corner, half_size) * gaussian(sample, width) * step;
}
return clamp(value, 0.0, 1.0);
}

float rounded_box_sdf(vec2 point, vec2 half_size, float corner) {
vec2 distance = abs(point) - half_size + vec2(corner);
return length(max(distance, 0.0)) + min(max(distance.x, distance.y), 0.0) - corner;
}

void main() {
vec2 point = vec2(gl_FragCoord.x, viewport_height - gl_FragCoord.y);
vec2 center = (shape.xy + shape.zw) * 0.5;
vec2 half_size = (shape.zw - shape.xy) * 0.5;
vec2 relative = point - center;
float coverage;

if (sigma <= 0.001) {
    float distance = rounded_box_sdf(relative, half_size, rounding);
    float antialias = max(fwidth(distance), 0.5);
    coverage = 1.0 - smoothstep(-antialias, antialias, distance);
} else {
    coverage = rounded_shadow(relative, half_size, sigma, rounding);
}

color = vec4(shadow_color.rgb, shadow_color.a * coverage);
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

static bool create_program() {
    const GLuint vertex = compile_shader(GL_VERTEX_SHADER, VERTEX_SHADER);
    const GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
    if (vertex == 0 || fragment == 0) {
        if (vertex != 0) glDeleteShader(vertex);
        if (fragment != 0) glDeleteShader(fragment);
        return false;
    }

    gl_state->program = glCreateProgram();
    glAttachShader(gl_state->program, vertex);
    glAttachShader(gl_state->program, fragment);
    glLinkProgram(gl_state->program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint linked = GL_FALSE;
    glGetProgramiv(gl_state->program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        glDeleteProgram(gl_state->program);
        gl_state->program = 0;
        return false;
    }

    gl_state->shape = glGetUniformLocation(gl_state->program, "shape");
    gl_state->rounding = glGetUniformLocation(gl_state->program, "rounding");
    gl_state->sigma = glGetUniformLocation(gl_state->program, "sigma");
    gl_state->viewport_height = glGetUniformLocation(gl_state->program, "viewport_height");
    gl_state->color = glGetUniformLocation(gl_state->program, "shadow_color");
    return true;
}

static bool ensure_gl_state() {
    if (gl_state->program == 0 && !create_program()) {
        return false;
    }

    if (gl_state->vertex_array == 0) {
        glGenVertexArrays(1, &gl_state->vertex_array);
    }

    return true;
}

static void render_box_shadow(const ImDrawList*, const ImDrawCmd* command) {
    const auto* region = static_cast<const ui::BoxShadowRegion*>(command->UserCallbackData);
    if (region == nullptr || !select_gl_state() || !ensure_gl_state()) {
        return;
    }

    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const int width = viewport[2];
    const int height = viewport[3];
    if (width <= 0 || height <= 0) {
        return;
    }

    const ImDrawData* draw_data = ImGui::GetDrawData();
    const ImVec2 display_position = draw_data == nullptr ? ImVec2{} : draw_data->DisplayPos;
    const ImVec2 scale = draw_data == nullptr ? ImVec2{1.0F, 1.0F} : draw_data->FramebufferScale;
    const auto to_framebuffer = [&](ImVec2 point) {
        return ImVec2{(point.x - display_position.x) * scale.x, (point.y - display_position.y) * scale.y};
    };

    const ImVec2 shape_min = to_framebuffer(region->shape.min);
    const ImVec2 shape_max = to_framebuffer(region->shape.max);
    const ImVec2 bounds_min = to_framebuffer(region->bounds.min);
    const ImVec2 bounds_max = to_framebuffer(region->bounds.max);
    const int left = std::clamp(static_cast<int>(std::floor(bounds_min.x)), 0, width);
    const int right = std::clamp(static_cast<int>(std::ceil(bounds_max.x)), 0, width);
    const int top = std::clamp(static_cast<int>(std::floor(bounds_min.y)), 0, height);
    const int bottom = std::clamp(static_cast<int>(std::ceil(bounds_max.y)), 0, height);
    const int clip_left =
        std::clamp(static_cast<int>(std::floor((command->ClipRect.x - display_position.x) * scale.x)), 0, width);
    const int clip_right =
        std::clamp(static_cast<int>(std::ceil((command->ClipRect.z - display_position.x) * scale.x)), 0, width);
    const int clip_top =
        std::clamp(static_cast<int>(std::floor((command->ClipRect.y - display_position.y) * scale.y)), 0, height);
    const int clip_bottom =
        std::clamp(static_cast<int>(std::ceil((command->ClipRect.w - display_position.y) * scale.y)), 0, height);
    const int clipped_left = std::max(left, clip_left);
    const int clipped_right = std::min(right, clip_right);
    const int clipped_top = std::max(top, clip_top);
    const int clipped_bottom = std::min(bottom, clip_bottom);
    if (clipped_right <= clipped_left || clipped_bottom <= clipped_top) {
        return;
    }

    const float scale_factor = std::min(scale.x, scale.y);
    const float shape_width = shape_max.x - shape_min.x;
    const float shape_height = shape_max.y - shape_min.y;
    const float radius = std::min(region->rounding * scale_factor, std::min(shape_width, shape_height) * 0.5F);
    const float blur = region->blur * 0.5F * scale_factor;

    glEnable(GL_SCISSOR_TEST);
    glScissor(clipped_left, height - clipped_bottom, clipped_right - clipped_left, clipped_bottom - clipped_top);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(gl_state->program);
    glUniform4f(gl_state->shape, shape_min.x, shape_min.y, shape_max.x, shape_max.y);
    glUniform1f(gl_state->rounding, radius);
    glUniform1f(gl_state->sigma, blur);
    glUniform1f(gl_state->viewport_height, static_cast<float>(height));
    glUniform4f(gl_state->color, region->color.x, region->color.y, region->color.z, region->color.w);
    glBindVertexArray(gl_state->vertex_array);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisable(GL_SCISSOR_TEST);
}

static bool initialize_box_shadow_effect(void*) {
    if (!GLAD_GL_VERSION_3_3 || !select_gl_state() || !ensure_gl_state()) {
        return false;
    }

    ui::set_box_shadow_callback(render_box_shadow);
    return true;
}

static void begin_box_shadow_effect(void*) {
    ui::begin_box_shadow_frame();
}

static void shutdown_box_shadow_effect(void*) {
    if (!select_gl_state()) {
        ui::shutdown_box_shadow();
        return;
    }

    ui::set_box_shadow_callback(nullptr);
    if (gl_state->program != 0) glDeleteProgram(gl_state->program);
    if (gl_state->vertex_array != 0) glDeleteVertexArrays(1, &gl_state->vertex_array);
    gl_states.erase(ImGui::GetCurrentContext());
    gl_state = nullptr;
    ui::shutdown_box_shadow();
}

void ui::register_opengl_box_shadow(EffectRegistry& effects) {
    effects.register_effect(
        {render_box_shadow, initialize_box_shadow_effect, begin_box_shadow_effect, shutdown_box_shadow_effect, nullptr}
    );
}
