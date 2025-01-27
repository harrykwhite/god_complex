#version 430 core

layout (location = 0) in vec2 a_vert;
layout (location = 1) in vec2 a_texCoord;

out vec2 v_texCoord;

void main() {
    gl_Position = vec4(a_vert, 0.0f, 1.0f);
    v_texCoord = a_texCoord;
}
