#version 450

layout(location = 0) in GS_DATA {
    vec4 vs_position;
    vec4 vs_normal;
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

void main() {
    out_albedo = vec4(u_push_constant.color.rgb, u_push_constant.pbr_info.x);
    out_normal = vec4(in_fs.vs_normal.xyz, u_push_constant.pbr_info.y);
    out_position = in_fs.vs_position;
    out_sun = vec4(0);
}
