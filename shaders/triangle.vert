#version 450

vec2 positions[] = vec2[]( vec2(-0.5), vec2(0.0, 0.5), vec2(0.5, -0.5) );

layout(push_constant) uniform push_constant_t {
    mat4 model;
    vec4 color;
    int texture_id;
} u_push_constant;

layout(set = 0, binding = 0) uniform camera_transforms_t {
    mat4 projection;
    mat4 view;
    mat4 view_projection;
} u_camera_transforms;

void main() {
     gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}