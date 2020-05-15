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

layout(set = 0, binding = 0) uniform camera_transforms_t {
    mat4 projection;
    mat4 view;
    mat4 inverse_view;
    mat4 view_projection;
    vec4 frustum;
    vec4 view_direction;
    mat4 previous_view_projection;
} u_camera_transforms;

layout(set = 1, binding = 0) uniform joint_transforms_t {
    mat4 transforms[25];
} joints;

#define MAX_WEIGHTS 4

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
	
    vec3 ws_position = vec3(u_push_constant.model * (accumulated_local));
    vec4 vs_position = u_camera_transforms.view * vec4(ws_position, 1.0);

    gl_Position = u_camera_transforms.projection * vs_position;
    out_vs.vs_position = vec3(vs_position);
    out_vs.vs_normal = vec3(u_camera_transforms.view * u_push_constant.model * vec4(accumulated_normal.xyz, 0.0f));
    out_vs.weights = in_weights;
}

#if 0
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
    out_vs.vs_position = vec3(u_camera_transforms.view * u_push_constant.model * vec4(in_position, 1.0f));
    out_vs.vs_normal = vec3(u_camera_transforms.view * u_push_constant.model * vec4(in_normal, 0.0f));
    out_vs.weights = in_weights;
                    
}
#endif
