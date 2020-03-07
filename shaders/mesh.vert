#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_uvs;

layout(location = 0) out VS_DATA {
    vec3 vs_position;
    vec3 vs_normal;
    vec2 uvs;
} out_vs;

layout(push_constant) uniform push_constant_t {
    mat4 model;
    vec4 color;
    vec4 pbr_info;
    int texture_id;
} u_push_constant;

layout(set = 0, binding = 0) uniform camera_transforms_t {
    mat4 projection;
    mat4 view;
    mat4 view_projection;
    vec4 frustum;
    vec4 view_direction;
} u_camera_transforms;

void main() {
    gl_Position = u_camera_transforms.view_projection * u_push_constant.model * vec4(in_position, 1.0f);
    out_vs.vs_normal = vec3(u_camera_transforms.view * vec4(in_position, 0.0f));
    out_vs.vs_position = vec3(u_camera_transforms.view * u_push_constant.model * vec4(in_position, 1.0f));
    out_vs.uvs = in_uvs;
}
