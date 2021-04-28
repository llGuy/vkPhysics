#version 450

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in VS_DATA {
    vec3 vs_position;
    vec3 vs_normal;
    vec4 weights;
} in_gs[];

layout(location = 0) out GS_DATA {
    vec3 vs_position;
    vec3 vs_normal;
    vec4 weights;
} out_gs;

layout(set = 0, binding = 0) uniform camera_transforms_t {
    mat4 projection;
    mat4 view;
    mat4 inverse_view;
    mat4 view_projection;
    vec4 frustum;
    vec4 view_direction;
    mat4 previous_view_projection;
} u_camera_transforms;

vec3 get_normal(int i1, int i2, int i3) {
    vec3 diff_world_pos1 = normalize(in_gs[i2].vs_position - in_gs[i1].vs_position);
    vec3 diff_world_pos2 = normalize(in_gs[i3].vs_position - in_gs[i2].vs_position);
    return(normalize(cross(diff_world_pos2, diff_world_pos1)));
}

void main() {
    vec3 normal = vec3(vec4(get_normal(0, 1, 2), 0.0));

    // vec3 a_to_b = in_gs[1].vs_position - in_gs[0].vs_position;
    // a_to_b /= 2.0f;
    // vec3 c_to_center = (a_to_b + in_gs[0].vs_position) - in_gs[2].vs_position;
    // vec3 center = in_gs[2].vs_position + c_to_center / 2.0f;
    // vec3 outer_vector = normalize(center - vec3(0.0f));

    for (int i = 0; i < 3; ++i) {
	out_gs.vs_normal = normalize(in_gs[0].vs_normal);
	out_gs.vs_position = in_gs[i].vs_position;
        out_gs.weights = in_gs[i].weights;

	gl_Position = gl_in[i].gl_Position;
        // gl_Position = u_camera_transforms.projection * vec4(in_gs[i].vs_position + outer_vector, 1.0f);
	
	EmitVertex();
    }
}
