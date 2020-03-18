#version 450

layout(location = 0) in VS_DATA {
    vec2 uvs;
} in_fs;

layout(location = 0) out float out_final_ao;

layout(set = 0, binding = 0) uniform sampler2D u_ao;

const float div_total = 1.0f / (4.0f * 4.0f);

void main() {
    vec2 texel_size = 1.0f / vec2(textureSize(u_ao, 0));
    float result = 0.0f;
    for (int x = -2; x < 2; ++x) {
        for (int y = -2; y < 2; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texel_size;
            result += texture(u_ao, in_fs.uvs + offset).r;
        }
    }

    out_final_ao = result * div_total;
}
