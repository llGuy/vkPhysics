#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_weights;
layout(location = 3) in ivec4 in_joint_ids;

layout(location = 0) out VS_DATA {
    vec3 vs_position;
    vec3 vs_normal;
    vec4 weights;
} out_vs;

layout(push_constant) uniform push_constant_t {
    mat4 model;
    vec4 color;
    vec4 pbr_info;
    int texture_id;
} u_push_constant;

layout(set = 0, binding = 0) uniform lighting_t {
    vec4 vs_light_positions[10];
    vec4 ws_light_positions[10];
    vec4 vs_light_directions[10];
    vec4 ws_light_directions[10];
    vec4 light_colors[10];
    vec4 vs_directional_light;
    vec4 ws_directional_light;

    mat4 shadow_view_projection;
    mat4 shadow_view;
    mat4 shadow_projection;

    vec2 light_screen_coord;
    
    int point_light_count;
} u_lighting_transforms;

layout(set = 1, binding = 0) uniform joint_transforms_t {
    mat4 transforms[25];
} joints;

#define MAX_WEIGHTS 4

mat4 rotate(
    float angle,
    vec3 axis) {
    float a = angle;
    float c = cos(a);
    float s = sin(a);

    axis = normalize(axis);
    vec3 temp = ((1.0f - c) * axis);

    mat4 rotate = mat4(1.0f);
    rotate[0][0] = c + temp[0] * axis[0];
    rotate[0][1] = temp[0] * axis[1] + s * axis[2];
    rotate[0][2] = temp[0] * axis[2] - s * axis[1];

    rotate[1][0] = temp[1] * axis[0] - s * axis[2];
    rotate[1][1] = c + temp[1] * axis[1];
    rotate[1][2] = temp[1] * axis[2] + s * axis[0];

    rotate[2][0] = temp[2] * axis[0] + s * axis[1];
    rotate[2][1] = temp[2] * axis[1] - s * axis[0];
    rotate[2][2] = c + temp[2] * axis[2];

    return rotate;
}

void main() {
    mat4 identity = mat4(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1);

    vec4 accumulated_local = vec4(0);
    vec4 accumulated_normal = vec4(0);

    for (int i = 0; i < MAX_WEIGHTS; ++i) {
        vec4 original_pos = vec4(in_position, 1.0f);

        mat4 joint = joints.transforms[in_joint_ids[i]];
		
        vec4 pose_position = joint * original_pos;

        accumulated_local += pose_position * in_weights[i];

        vec4 world_normal = joint * vec4(in_normal, 0.0f);
        accumulated_normal += world_normal * in_weights[i];
    }

    mat4 rotation_correction = rotate(radians(-90.0f), vec3(1, 0, 0));
    
    vec3 ws_position = vec3(u_push_constant.model * rotation_correction * (accumulated_local));

    gl_Position = u_lighting_transforms.shadow_view_projection * vec4(ws_position, 1.0f);
}
