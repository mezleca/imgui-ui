#include <imgui-ui/imgui/effects/gradient/opengl-shared.hpp>

ui::GradientUniforms ui::find_gradient_uniforms(GLuint program) {
    return {
        glGetUniformLocation(program, "solid_color"),    glGetUniformLocation(program, "gradient_type"),
        glGetUniformLocation(program, "gradient_start"), glGetUniformLocation(program, "gradient_end"),
        glGetUniformLocation(program, "stop_count"),     glGetUniformLocation(program, "stop_positions[0]"),
        glGetUniformLocation(program, "stop_colors[0]"),
    };
}

void ui::upload_gradient(const GradientUniforms& uniforms, ImVec4 solid, const GradientData& gradient) {
    glUniform4f(uniforms.solid, solid.x, solid.y, solid.z, solid.w);
    glUniform1i(uniforms.type, static_cast<int>(gradient.type));
    glUniform2f(uniforms.start, gradient.start.x, gradient.start.y);
    glUniform2f(uniforms.end, gradient.end.x, gradient.end.y);
    glUniform1i(uniforms.count, gradient.count);

    if (gradient.count == 0) return;
    float positions[MAX_GRADIENT_STOPS]{};
    float colors[MAX_GRADIENT_STOPS * 4]{};
    for (uint8_t index = 0; index < gradient.count; ++index) {
        const GradientStop& stop = gradient.stops[index];
        positions[index] = stop.position;
        colors[index * 4] = stop.color.x;
        colors[index * 4 + 1] = stop.color.y;
        colors[index * 4 + 2] = stop.color.z;
        colors[index * 4 + 3] = stop.color.w;
    }
    glUniform1fv(uniforms.positions, gradient.count, positions);
    glUniform4fv(uniforms.colors, gradient.count, colors);
}
