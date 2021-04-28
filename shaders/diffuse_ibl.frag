#version 450

layout(location = 0) in vec2 in_uvs;
layout(location = 1) in flat int in_face_index;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform samplerCube cubemap;

const vec3 PREMADE_DIRECTIONS[] = vec3[] (
    vec3(1.0f, 0.0f, 0.0f),
    vec3(-1.0f, 0.0f, 0.0f),
    vec3(0.000000001f, +1.0f, 0.00000000f),
    vec3(0.000000001f, -1.0f, 0.00000000f),
    vec3(-1.0f, 0.0f, 0.0f),
    vec3(+1.0f, 0.0f, 0.0f));

vec3 compute_normal() {
    if (in_face_index == 0) {
        return normalize(vec3(+1.0f, -in_uvs.y, -in_uvs.x));
    }
    else if (in_face_index == 1) {
        return normalize(vec3(-1.0f, -in_uvs.y, +in_uvs.x));
    }
    else if (in_face_index == 2) {
        return normalize(vec3(+in_uvs.x, +1.0, +in_uvs.y));
    }
    else if (in_face_index == 3) {
        return normalize(vec3(+in_uvs.x, -1.0, -in_uvs.y));
    }
    else if (in_face_index == 4) {
        return normalize(vec3(+in_uvs.x, -in_uvs.y, +1.0));
    }
    else if (in_face_index == 5) {
        return normalize(vec3(-in_uvs.x, -in_uvs.y, -1.0));
    }
}

void main() {
    vec3 normal = compute_normal();

    out_color = vec4(0);
    vec3 up = vec3(0, 1, 0);
    vec3 right = normalize(cross(normal, up));
    up = cross(normal, right);

    float index = 0;

    for (float phi = 0; phi < 6.283; phi += 0.025) {
        for (float theta = 0; theta < 1.57; theta += 0.1) {
            vec3 temp = cos(phi) * right + sin(phi) * up;
            vec3 vector = cos(theta) * normal + sin(theta) * temp;
            out_color += texture(cubemap, vector) * cos(theta) * sin(theta);
            index += 1.0;
        }
    }

    out_color = vec4(3.1415 * out_color / index);
    out_color.a = 1.0;
}
