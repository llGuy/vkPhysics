#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_weights;
layout(location = 3) in ivec4 in_joint_ids;

layout(location = 0) out VS_DATA {
    vec3 position;
    vec3 normal;
    vec4 weights;
    ivec4 joint_ids;
} out_vs;

void main() {
    // Here don't calculate anything, leave that to the geometry shader
    out_vs.position = in_position;
    out_vs.normal = in_normal;
    out_vs.weights = in_weights;
    out_vs.joint_ids = in_joint_ids;

    // gl_Position = vec4(0);
    gl_Position = vec4(out_vs.position, 1.0f);
}
