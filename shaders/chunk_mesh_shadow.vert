#version 450

layout(location = 0) in uint in_low;
layout(location = 1) in uint in_high;

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
    uint ix = in_low >> 28;
    uint iy = (in_low >> 24) & 15;
    uint iz = (in_low >> 20) & 15;

    uint inormalized_x = (in_low >> 12) & 255;
    uint inormalized_y = (in_low >> 4) & 255;
    uint inormalized_z = ((in_low) & 15) << 4;
    inormalized_z += (in_high >> 28);

    vec3 position = vec3(float(ix), float(iy), float(iz));
    position += vec3(float(inormalized_x), float(inormalized_y), float(inormalized_z)) / 255.0f;

    gl_Position = u_lighting_transforms.shadow_view_projection * u_push_constant.model * vec4(position, 1.0f);
    out_vs.depth = gl_Position.z / gl_Position.w;
}
