#version 450

// layout(location = 0) in vec3 in_position;
// layout(location = 1) in uint in_color;

layout(location = 0) in uint in_low;
layout(location = 1) in uint in_high;

layout(location = 0) out VS_DATA {
    vec4 vs_position;
    vec4 ws_position;
    vec3 color;
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
    uint ix = in_low >> 28;
    uint iy = (in_low >> 24) & 15;
    uint iz = (in_low >> 20) & 15;

    uint inormalized_x = (in_low >> 12) & 255;
    uint inormalized_y = (in_low >> 4) & 255;
    uint inormalized_z = ((in_low) & 15) << 4;
    inormalized_z += (in_high >> 28);

    uint color = (in_high >> 20) & 255;

    uint r_b8 = color >> 5;
    uint g_b8 = (color >> 2) & 7;
    uint b_b8 = (color) & 3;

    float r_f32 = float(r_b8) / float(7);
    float g_f32 = float(g_b8) / float(7);
    float b_f32 = float(b_b8) / float(3);

    vec3 position = vec3(float(ix), float(iy), float(iz));
    position += vec3(float(inormalized_x), float(inormalized_y), float(inormalized_z)) / 255.0f;

    vec4 ms_position = u_push_constant.model * vec4(position, 1.0f);
    gl_Position = u_camera_transforms.view_projection * ms_position;
    out_vs.vs_position = u_camera_transforms.view * vec4(ms_position.xyz, 1.0f);
    out_vs.ws_position = ms_position;
    out_vs.color = vec3(r_f32, g_f32, b_f32);
}
