#version 450

layout(location = 0) out VS_DATA {
    vec3 cubemap_direction;
    vec3 vs_position;
} out_vs;

const vec3 VERTICES[] = vec3[] (
    vec3(-1.0f, -1.0f, 1.0f),
    vec3(1.0f, -1.0f, 1.0f),
    vec3(1.0f, 1.0f, 1.0f),
    vec3(-1.0f, 1.0f, 1.0f),

    vec3(-1.0f, -1.0f, -1.0f),
    vec3(1.0f, -1.0f, -1.0f),
    vec3(1.0f, 1.0f, -1.0f),
    vec3(-1.0f, 1.0f, -1.0f));

const int INDICES[] = int[] (
    0, 1, 2,
    2, 3, 0,

    1, 5, 6,
    6, 2, 1,

    7, 6, 5,
    5, 4, 7,
	    
    3, 7, 4,
    4, 0, 3,
	    
    4, 5, 1,
    1, 0, 4,
	    
    3, 2, 6,
    6, 7, 3);

layout(push_constant) uniform push_constant_t {
    mat4 model;
    float invert_y;
} u_push_constant;

layout(set = 0, binding = 0) uniform camera_transforms_t {
    mat4 projection;
    mat4 view;
    mat4 inverse_view;
    mat4 view_projection;
    vec4 frustum;
    vec4 view_direction;
} u_camera_transforms;

void main() {
    vec3 ms_vertex_position = VERTICES[INDICES[gl_VertexIndex]];

    mat4 view_rotation = u_camera_transforms.view;
    view_rotation[3][0] = 0.0f;
    view_rotation[3][1] = 0.0f;
    view_rotation[3][2] = 0.0f;

    vec4 vs_position = view_rotation * vec4(ms_vertex_position * 1000.0f, 1.0f);

    gl_Position = u_camera_transforms.projection * vs_position;

    out_vs.cubemap_direction = normalize(ms_vertex_position);
    out_vs.cubemap_direction.y *= u_push_constant.invert_y;
    out_vs.vs_position = vs_position.xyz;
}
