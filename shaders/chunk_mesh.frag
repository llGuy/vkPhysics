#version 450

layout(location = 0) in GS_DATA {
    vec4 vs_position;
    vec4 ws_position;
    vec4 vs_normal;
    vec4 color;
} in_fs;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_position;
layout(location = 3) out vec4 out_sun;

layout(push_constant) uniform push_constant_t {
    mat4 model;
    vec4 color;
    vec4 pbr_info;
    int texture_id;
} u_push_constant;

layout(set = 1, binding = 0) uniform chunk_color_data_t {
    // Color of zone
    vec4 pointer_position;
    vec4 pointer_color;
    float pointer_radius;
} u_chunk_color_data;

void main() {
    vec3 pointer_diff = vec3(in_fs.ws_position) - vec3(u_chunk_color_data.pointer_position);
    float dist2 = dot(pointer_diff, pointer_diff);

    out_albedo = vec4(in_fs.color.rgb, u_push_constant.pbr_info.x);

    if (dist2 < u_chunk_color_data.pointer_radius * u_chunk_color_data.pointer_radius) {
        float quot = 1.0f - dist2 / (u_chunk_color_data.pointer_radius * u_chunk_color_data.pointer_radius);
        quot = pow(quot, 3);
        out_albedo = mix(out_albedo, vec4(u_chunk_color_data.pointer_color.rgb, 1.0f), quot);
    }

    out_normal = vec4(in_fs.vs_normal.xyz, u_push_constant.pbr_info.y);
    out_position = in_fs.vs_position;
    out_sun = vec4(0);
}
