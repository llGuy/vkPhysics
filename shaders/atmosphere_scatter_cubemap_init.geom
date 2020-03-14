#version 450

layout(points) in;
layout(triangle_strip, max_vertices = 24) out;

layout(location = 0) out GS_DATA {
    vec3 normal;
    vec3 cube_face_direction;
} out_gs;

/* 0: GL_TEXTURE_CUBE_MAP_POSITIVE_X
   1: GL_TEXTURE_CUBE_MAP_NEGATIVE_X
   2: GL_TEXTURE_CUBE_MAP_POSITIVE_Y
   3: GL_TEXTURE_CUBE_MAP_NEGATIVE_Y
   4: GL_TEXTURE_CUBE_MAP_POSITIVE_Z
   5: GL_TEXTURE_CUBE_MAP_NEGATIVE_Z */

const vec3 PREMADE_DIRECTIONS[] = vec3[] (vec3(0, 0, -1),
					  vec3(0, 0, +1),
					  vec3(0.000000001, +1, 0.00000000),
					  vec3(0.000000001, -1, 0.00000000),
					  vec3(-1, 0, 0),
					  vec3(+1, 0, 0));

const vec3 VERTICES[] = vec3[] (
    vec3(-1.0f, -1.0f, 1.0f), // 0
    vec3(1.0f, -1.0f, 1.0f),  // 1
    vec3(1.0f, 1.0f, 1.0f),   // 2
    vec3(-1.0f, 1.0f, 1.0f),  // 3

    vec3(-1.0f, -1.0f, -1.0f),// 4
    vec3(1.0f, -1.0f, -1.0f), // 5
    vec3(1.0f, 1.0f, -1.0f),  // 6
    vec3(-1.0f, 1.0f, -1.0f));// 7

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

void main() {
    int counter = 0;
    for (int face_idx = 0; face_idx < 6; ++face_idx) {
	for (int vertex_idx = 0; vertex_idx < 4; ++vertex_idx) {
	    gl_Position = vec4(vec2(1.0 - float((vertex_idx << 1) & 2), 1.0 - (vertex_idx & 2)) * 2.0f - 1.0f, 0.0f, 1.0f);
	    gl_Layer = face_idx;
	    out_gs.cube_face_direction = PREMADE_DIRECTIONS[face_idx];
            out_gs.normal = VERTICES[INDICES[counter]];
	    EmitVertex();

            ++counter;
	}
	EndPrimitive();
    }
}
