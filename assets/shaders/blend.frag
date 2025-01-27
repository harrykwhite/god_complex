#version 430 core

in vec2 v_texCoord;
out vec4 o_fragColor;

uniform sampler2D u_tex;

uniform vec3 u_color;
uniform float u_intensity;

void main() {
    vec4 color = texture(u_tex, v_texCoord);

    o_fragColor = vec4(
        mix(color.r, u_color.r, u_intensity),
        mix(color.g, u_color.g, u_intensity),
        mix(color.b, u_color.b, u_intensity),
        color.a
    );
}
