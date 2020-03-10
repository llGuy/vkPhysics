#version 450

layout(points) in;
layout(triangle_strip, max_vertices = 24) out;

layout(location = 0) out vec2 normal_xy;
layout(location = 1) out flat int face_index;

const vec3 PREMADE_DIRECTIONS[] = vec3[] (
    vec3(1.0f, 0.0f, 0.0f),
    vec3(-1.0f, 0.0f, 0.0f),
    vec3(0.000000001f, +1.0f, 0.00000000f),
    vec3(0.000000001f, -1.0f, 0.00000000f),
    vec3(-1.0f, 0.0f, 0.0f),
    vec3(+1.0f, 0.0f, 0.0f));

const vec2 NORMAL_XY[] = vec2[] (
    vec2(-1.0f, -1.0f),
    vec2(-1.0f, 1.0f),
    vec2(1.0f, -1.0f),
    vec2(1.0f, 1.0f) );

void main() {
    for (int face_idx = 0; face_idx < 6; ++face_idx) {
	for (int vertex_idx = 0; vertex_idx < 4; ++vertex_idx) {
            gl_Position = vec4(NORMAL_XY[vertex_idx], 0, 1);
	    gl_Layer = face_idx;
            normal_xy = NORMAL_XY[vertex_idx];
            face_index = face_idx;
            
	    EmitVertex();
	}
	EndPrimitive();
    }
}
