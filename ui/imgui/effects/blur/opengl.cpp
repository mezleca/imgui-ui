#include "opengl.hpp"

#include "blur.hpp"
#include "../effects.hpp"

#include <glad/gl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>

using namespace ui;

struct BlurTextures {
    GLuint source = 0;
    GLuint ping = 0;
    GLuint pong = 0;
    GLuint result = 0;
    GLuint framebuffer = 0;
    GLuint vertex_array = 0;
    GLuint program = 0;
    int width = 0;
    int height = 0;
    int strength = -1;
    bool captured = false;
};

static std::unordered_map<ImGuiContext*, BlurTextures> texture_sets;
static BlurTextures* textures = nullptr;

static bool select_textures() {
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr) {
        textures = nullptr;
        return false;
    }

    textures = &texture_sets[context];
    return true;
}

static constexpr const char* VERTEX_SHADER = R"(#version 330 core
void main() {
const vec2 positions[3] = vec2[](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
})";

static constexpr const char* FRAGMENT_SHADER = R"(#version 330 core
uniform sampler2D image;
uniform sampler2D original;
uniform vec2 texel;
uniform vec2 direction;
uniform vec4 region;
uniform float rounding;
uniform int radius;
uniform bool filtered;
uniform bool clipped;
uniform float opacity;
uniform float amount;
out vec4 color;

float rounded_box(vec2 point, vec2 size, float radius) {
vec2 distance = abs(point - size * 0.5) - size * 0.5 + radius;
return length(max(distance, 0.0)) + min(max(distance.x, distance.y), 0.0) - radius;
}

vec4 box_blur(vec2 uv, vec2 step) {
vec4 sum = texture(image, uv);
for (int pair = 0; pair < 32; ++pair) {
    if (pair >= radius / 2) break;
    float distance = float(pair * 2) + 1.5;
    sum += (texture(image, uv - step * distance) + texture(image, uv + step * distance)) * 2.0;
}

if ((radius & 1) != 0) {
    sum += texture(image, uv - step * radius) + texture(image, uv + step * radius);
}

return sum / float(radius * 2 + 1);
}

void main() {
vec2 uv = gl_FragCoord.xy * texel;
vec4 blurred = filtered ? box_blur(uv, texel * direction) : texture(image, uv);
if (!clipped) {
    color = blurred;
    return;
}

blurred = mix(texture(original, uv), blurred, amount);

vec2 point = gl_FragCoord.xy - region.xy;
if (rounded_box(point, region.zw, rounding) > 0.0) discard;
color = vec4(blurred.rgb, blurred.a * opacity);
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

    textures->program = glCreateProgram();

    glAttachShader(textures->program, vertex);
    glAttachShader(textures->program, fragment);
    glLinkProgram(textures->program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint linked = GL_FALSE;
    glGetProgramiv(textures->program, GL_LINK_STATUS, &linked);
    if (linked == GL_TRUE) {
        return true;
    }

    glDeleteProgram(textures->program);
    textures->program = 0;
    return false;
}

static void create_texture(GLuint& texture, int width, int height) {
    if (texture == 0) glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
}

static bool ensure_textures(int width, int height) {
    if (textures->program == 0 && !create_program()) {
        return false;
    }

    if (textures->width == width && textures->height == height) {
        return true;
    }

    textures->width = width;
    textures->height = height;

    create_texture(textures->source, width, height);
    create_texture(textures->ping, width, height);
    create_texture(textures->pong, width, height);

    if (textures->framebuffer == 0) glGenFramebuffers(1, &textures->framebuffer);
    if (textures->vertex_array == 0) glGenVertexArrays(1, &textures->vertex_array);

    return true;
}

static void blur_pass(GLuint input, GLuint output, int width, int height, int radius, float direction_x, float direction_y) {
    glBindFramebuffer(GL_FRAMEBUFFER, textures->framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return;
    glViewport(0, 0, width, height);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glUseProgram(textures->program);
    glUniform1i(glGetUniformLocation(textures->program, "image"), 0);
    glUniform2f(glGetUniformLocation(textures->program, "texel"), 1.0F / width, 1.0F / height);
    glUniform2f(glGetUniformLocation(textures->program, "direction"), direction_x, direction_y);
    glUniform1i(glGetUniformLocation(textures->program, "radius"), radius);
    glUniform1i(glGetUniformLocation(textures->program, "filtered"), GL_TRUE);
    glUniform1i(glGetUniformLocation(textures->program, "clipped"), GL_FALSE);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, input);
    glBindVertexArray(textures->vertex_array);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

static std::array<int, 3> box_widths(int sigma) {
    constexpr int passes = 3;
    // https://drafts.csswg.org/filter-effects/#funcdef-filter-blur
    // three box widths approximate a gaussian whose standard deviation is sigma.
    int lower = static_cast<int>(std::floor(std::sqrt((12.0 * sigma * sigma / passes) + 1.0)));
    if ((lower & 1) == 0) --lower;

    const int upper = lower + 2;
    const int lower_count = static_cast<int>(
        std::round((12.0 * sigma * sigma - passes * lower * lower - 4.0 * passes * lower - 3.0 * passes) / (-4.0 * lower - 4.0))
    );

    return {
        lower_count > 0 ? lower : upper,
        lower_count > 1 ? lower : upper,
        lower_count > 2 ? lower : upper,
    };
}

static void blur(int width, int height, int sigma) {
    const std::array<int, 3> widths = box_widths(sigma);
    GLuint input = textures->source;
    GLuint output = textures->ping;

    for (const int window : widths) {
        blur_pass(input, output, width, height, window / 2, 1.0F, 0.0F);
        std::swap(input, output);
    }

    for (const int window : widths) {
        blur_pass(input, output, width, height, window / 2, 0.0F, 1.0F);
        std::swap(input, output);
    }

    textures->result = input;
}

static void render_blur(const ImDrawList*, const ImDrawCmd* command) {
    const auto* region = static_cast<const BlurRegion*>(command->UserCallbackData);

    if (region == nullptr || !select_textures()) {
        return;
    }

    GLint viewport[4]{};
    GLint framebuffer = 0;

    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &framebuffer);

    const int width = viewport[2];
    const int height = viewport[3];

    if (width <= 0 || height <= 0 || !ensure_textures(width, height)) {
        return;
    }

    if (!textures->captured) {
        glBindTexture(GL_TEXTURE_2D, textures->source);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);

        textures->captured = true;
    }

    if (textures->strength != region->strength) {
        blur(width, height, region->strength);
        textures->strength = region->strength;
    }

    const ImDrawData* draw_data = ImGui::GetDrawData();
    const ImVec2 display_position = draw_data == nullptr ? ImVec2{} : draw_data->DisplayPos;
    const ImVec2 scale = draw_data == nullptr ? ImVec2{1.0F, 1.0F} : draw_data->FramebufferScale;
    const ImVec2 minimum = {
        (region->rect.min.x - display_position.x) * scale.x,
        (region->rect.min.y - display_position.y) * scale.y,
    };
    const ImVec2 maximum = {
        (region->rect.max.x - display_position.x) * scale.x,
        (region->rect.max.y - display_position.y) * scale.y,
    };

    const int left = std::clamp(static_cast<int>(std::floor(minimum.x)), 0, width);
    const int bottom = std::clamp(static_cast<int>(std::floor(height - maximum.y)), 0, height);
    const int region_width = std::clamp(static_cast<int>(std::ceil(maximum.x - minimum.x)), 0, width - left);
    const int region_height = std::clamp(static_cast<int>(std::ceil(maximum.y - minimum.y)), 0, height - bottom);
    if (region_width <= 0 || region_height <= 0) {
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(framebuffer));
    glViewport(0, 0, width, height);
    glEnable(GL_SCISSOR_TEST);
    glScissor(left, bottom, region_width, region_height);
    glDisable(GL_BLEND);
    glUseProgram(textures->program);
    glUniform1i(glGetUniformLocation(textures->program, "image"), 0);
    glUniform1i(glGetUniformLocation(textures->program, "original"), 1);
    glUniform2f(glGetUniformLocation(textures->program, "texel"), 1.0F / width, 1.0F / height);
    glUniform4f(
        glGetUniformLocation(textures->program, "region"), static_cast<float>(left), static_cast<float>(bottom),
        static_cast<float>(region_width), static_cast<float>(region_height)
    );
    glUniform1f(
        glGetUniformLocation(textures->program, "rounding"),
        std::min(region->rounding * std::min(scale.x, scale.y), std::min(region_width, region_height) * 0.5F)
    );
    glUniform1f(glGetUniformLocation(textures->program, "opacity"), region->opacity);
    glUniform1f(
        glGetUniformLocation(textures->program, "amount"),
        static_cast<float>(region->strength) / (static_cast<float>(region->strength) + 4.0F)
    );
    glUniform1i(glGetUniformLocation(textures->program, "clipped"), GL_TRUE);
    glUniform1i(glGetUniformLocation(textures->program, "filtered"), GL_FALSE);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures->result);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textures->source);
    glBindVertexArray(textures->vertex_array);

    if (region->opacity < 1.0F) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }

    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisable(GL_SCISSOR_TEST);
    glActiveTexture(GL_TEXTURE0);
}

static bool initialize_blur_effect(void*) {
    if (!GLAD_GL_VERSION_3_3 || !select_textures()) {
        return false;
    }

    set_blur_callback(render_blur);
    return true;
}

static void begin_blur_effect(void*) {
    if (!select_textures()) {
        return;
    }

    textures->captured = false;
    textures->strength = -1;
}

static void shutdown_blur_effect(void*) {
    if (!select_textures()) {
        shutdown_blur();
        return;
    }

    set_blur_callback(nullptr);

    if (textures->program != 0) glDeleteProgram(textures->program);
    if (textures->vertex_array != 0) glDeleteVertexArrays(1, &textures->vertex_array);
    if (textures->framebuffer != 0) glDeleteFramebuffers(1, &textures->framebuffer);
    if (textures->source != 0) glDeleteTextures(1, &textures->source);
    if (textures->ping != 0) glDeleteTextures(1, &textures->ping);
    if (textures->pong != 0) glDeleteTextures(1, &textures->pong);

    texture_sets.erase(ImGui::GetCurrentContext());
    textures = nullptr;
    shutdown_blur();
}

void ui::register_opengl_blur(EffectRegistry& effects) {
    effects.register_effect({render_blur, initialize_blur_effect, begin_blur_effect, shutdown_blur_effect, nullptr});
}
