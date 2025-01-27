#version 430 core

in vec2 v_tex_coord;
out vec4 o_frag_color;

uniform sampler2D u_tex;

uniform vec2 u_cam_topleft;
uniform vec2 u_cam_size;

uniform float u_darkness;

uniform vec2 u_light_pos;
uniform float u_light_radius;

void main() {
    vec2 pos = u_cam_topleft + vec2(v_tex_coord.x * u_cam_size.x, (1.0 - v_tex_coord.y) * u_cam_size.y);
    float light_dist = distance(pos, u_light_pos);
    float brightness = (1.0 - (u_darkness * clamp(light_dist / u_light_radius, 0.0, 1.0)));

    o_frag_color = texture(u_tex, v_tex_coord) * vec4(brightness, brightness, brightness, 1.0);
}
