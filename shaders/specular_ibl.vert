#version 450

layout(location = 0) out VS_DATA {
    vec3 local_position;
} out_vs;

vec3 local_positions[] = vec3[](
    vec3(-1.0f, -1.0f, 1.0f),      // 0 
    vec3(1.0f, -1.0f, 1.0f),       // 1
    vec3(1.0f, 1.0f, 1.0f),        // 2
    vec3(-1.0f, 1.0f, 1.0f),       // 3
    vec3(-1.0f, -1.0f, -1.0f),     // 4
    vec3(1.0f, -1.0f, -1.0f),      // 5
    vec3(1.0f, 1.0f, -1.0f),       // 6
    vec3(-1.0f, 1.0f, -1.0f) );    // 7

uint mesh_indices[] = uint[](
    7, 6, 5,  // -z
    5, 4, 7,   
    
    0, 1, 2,  // +z
    2, 3, 0,   
    
    3, 2, 6,  // +y
    6, 7, 3,
    
    4, 5, 1,  // -y
    1, 0, 4,
    
    3, 7, 4,  // -x
    4, 0, 3,
    
    1, 5, 6,  // +x
    6, 2, 1);

layout(push_constant) uniform push_constant_t {
    mat4 matrix;
    float roughness;
    float layer;
} u_push_constant;

void main() {
    out_vs.local_position = local_positions[mesh_indices[gl_VertexIndex]];
    
    gl_Position = u_push_constant.matrix * vec4(out_vs.local_position, 1.0f);
}
