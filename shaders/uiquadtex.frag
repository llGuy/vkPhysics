#version 450

layout(location = 0) out vec4 final_color;

layout(location = 0) in VS_DATA {
    vec4 color;
    vec2 uvs;
} fs_in;

layout(binding = 0, set = 0) uniform sampler2D u_texture;

void main(void) {
    final_color = vec4(fs_in.color.rgb, texture(u_texture, fs_in.uvs).a);
}
