#version 450

layout (location = 0) out VS_DATA {
    vec2 uvs;
} out_vs;

const vec2 UVS[] = vec2[]( vec2(0, 0), vec2(0, 1), vec2(1, 0), vec2(1, 1) );

void main() {
    out_vs.uvs = UVS[gl_VertexIndex];
    gl_Position = vec4(out_vs.uvs * 2.0f - 1.0f, 0.0f, 1.0f);
}
