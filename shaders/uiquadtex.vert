#version 450

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_uvs;
layout(location = 2) in uint in_color;

layout(location = 0) out VS_DATA {
    vec4 color;
    vec2 uvs;
} vs_out;

vec4 uint_color_to_v4(
    uint color) {
    float r = float((color >> 24) & 0xff) / 256.0;
    float g = float((color >> 16) & 0xff) / 256.0;
    float b = float((color >> 8) & 0xff) / 256.0;
    float a = float(color & 0xff) / 256.0;
    return(vec4(r, g, b, a));
}

void main() {
    vs_out.color = uint_color_to_v4(in_color);
    vs_out.uvs = in_uvs;
    gl_Position = vec4(in_position, 0.0f, 1.0f);
    gl_Position.y *= -1.0f;
}
