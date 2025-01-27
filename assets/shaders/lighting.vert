#version 430 core

layout (location = 0) in vec2 a_vert;
layout (location = 1) in vec2 a_tex_coord;

out vec2 v_tex_coord;

void main() {
    gl_Position = vec4(a_vert, 0.0f, 1.0f);
    v_tex_coord = a_tex_coord;
}
