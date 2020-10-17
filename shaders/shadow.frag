#version 450

layout(location = 0) out vec2 out_moment;

layout(location = 0) in VS_DATA {
    float depth;
} in_fs;

void main() {
    out_moment = vec2(gl_FragCoord.z, gl_FragCoord.z * gl_FragCoord.z);
}
