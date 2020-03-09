#version 450

layout(location = 0) in VS_DATA {
    vec3 cubemap_direction;
} in_fs;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_position;

layout(set = 1, binding = 0) uniform samplerCube cubemap;

void main() {
    out_albedo = texture(cubemap, in_fs.cubemap_direction);
    // A normal vector value of -10.0f means that no lighting calculations should be made
    out_normal = vec4(-10.0f);
    out_position = vec4(-10.0f);
}
