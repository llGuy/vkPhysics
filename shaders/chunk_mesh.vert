#version 450

layout(location = 0) in vec3 in_position;

layout(location = 0) out VS_DATA {
    vec4 vs_position;
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
    mat4 inverse_view;
    mat4 view_projection;
    vec4 frustum;
    vec4 view_direction;
    mat4 previous_view_projection;
} u_camera_transforms;

void main() {
    gl_Position = u_camera_transforms.view_projection * u_push_constant.model * vec4(in_position, 1.0f);
    out_vs.vs_position = u_camera_transforms.view * vec4(in_position, 1.0f);
}
