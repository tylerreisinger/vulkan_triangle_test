#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform Transformations {
    mat4 perspective;
    mat4 view;
    mat4 model;
} trans;

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;
layout(location = 0) out vec4 fragColor;

out gl_PerVertex {
    vec4 gl_Position;
};


void main() {
    gl_Position = trans.perspective * trans.view * trans.model * vec4(position, 1.0);
    fragColor = color;
}
