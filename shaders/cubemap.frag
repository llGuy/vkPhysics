#version 450

layout(location = 0) in VS_DATA {
    vec3 cubemap_direction;
    vec3 vs_position;
} in_fs;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_position;
layout(location = 3) out vec4 out_sun;

layout(set = 1, binding = 0) uniform samplerCube cubemap;

void main() {
    vec4 c = texture(cubemap, in_fs.cubemap_direction);
    if (c.a == 1.0f) {
        out_sun = c;
    }
    else {
        out_sun = vec4(0);
    }

    out_albedo = c;
    // A normal vector value of -10.0f means that no lighting calculations should be made
    out_normal = vec4(-10.0f);
    out_position = vec4(in_fs.vs_position, 1.0f);
}
