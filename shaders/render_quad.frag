#version 450

layout(location = 0) in VS_DATA {
    vec2 uvs;
} in_fs;

layout(location = 0) out vec4 out_final_color;

layout(binding = 0, set = 0) uniform sampler2D u_diffuse;

void main() {
    out_final_color = texture(u_diffuse, in_fs.uvs);// + texture(u_bloom, in_fs.uvs);
}
