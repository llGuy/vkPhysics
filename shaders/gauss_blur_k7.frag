#version 450

layout (location = 0) in VS_DATA {
    vec2 uvs;
} in_fs;

layout(location = 0) out vec2 out_final_color;

layout(push_constant) uniform push_constant_t {
    uint horizontal;
    vec2 scale;
} u_push_constant;

layout(binding = 0, set = 0) uniform sampler2D u_diffuse;

// float weights[4] = float[](0.383103, 0.241843, 0.060626, 0.00598);
float weights[4] = float[](20.0 / 64.0, 15.0 / 64.0, 6.0 / 64.0, 1.0 / 64.0);

void main() {
#if 1
    // vec2 tex_offset = 1.0f / (textureSize(u_diffuse, 0));
    vec2 tex_offset = u_push_constant.scale;

    vec3 color = texture(u_diffuse, in_fs.uvs).rgb * weights[0];

    if (bool(u_push_constant.horizontal)) {
        for (int i = 1; i < 4; ++i) {
            color += texture(u_diffuse, in_fs.uvs + vec2(tex_offset.x * i, 0.0f)).rgb * weights[i];
            color += texture(u_diffuse, in_fs.uvs - vec2(tex_offset.x * i, 0.0f)).rgb * weights[i];
        }
    }
    else {
        for (int i = 1; i < 4; ++i){
            color += texture(u_diffuse, in_fs.uvs + vec2(0.0f, tex_offset.x * i)).rgb * weights[i];
            color += texture(u_diffuse, in_fs.uvs - vec2(0.0f, tex_offset.x * i)).rgb * weights[i];
        }
    }

    out_final_color = color.xy;
#endif
}
