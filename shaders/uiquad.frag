#version 450

layout(location = 0) in VS_DATA {
    vec4 color;
} fs_in;

layout(location = 0) out vec4 final_color;

void main() {
    final_color = fs_in.color;
}
