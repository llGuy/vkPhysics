#version 450

layout(location = 0) in VS_DATA {
    vec2 uvs;
} fs_in;

layout(location = 0) out vec4 final_color;

layout(binding = 0, set = 0) uniform sampler2D input_tx;

void main() {
    final_color = texture(input_tx, fs_in.uvs);
}
