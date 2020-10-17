#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in uint in_color;

layout(location = 0) out VS_DATA {
    float depth;
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

void main() {
    gl_Position = u_lighting_transforms.shadow_view_projection * u_push_constant.model * vec4(in_position, 1.0f);
    out_vs.depth = gl_Position.z / gl_Position.w;
}
