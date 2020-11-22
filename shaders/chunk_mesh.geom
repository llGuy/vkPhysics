#version 450

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in VS_DATA {
    vec4 vs_position;
    vec4 ws_position;
    vec3 color;
} in_gs[];

layout(location = 0) out GS_DATA {
    vec4 vs_position;
    vec4 ws_position;
    vec4 vs_normal;
    vec4 color;
} out_gs;

vec3 get_normal(int i1, int i2, int i3) {
    vec3 diff_world_pos1 = normalize(in_gs[i2].vs_position.xyz - in_gs[i1].vs_position.xyz);
    vec3 diff_world_pos2 = normalize(in_gs[i3].vs_position.xyz - in_gs[i2].vs_position.xyz);
    return(normalize(cross(diff_world_pos2, diff_world_pos1)));
}

const float MAX_R_B8 = 7.0;
const float MAX_G_B8 = 7.0;
const float MAX_B_B8 = 3.0;

void main() {
    vec3 normal = -get_normal(0, 1, 2);
    vec3 color = in_gs[0].color;

    for (int i = 0; i < 3; ++i) {
        out_gs.vs_normal = vec4(normal, 0.0f);
        out_gs.vs_position = in_gs[i].vs_position;
        out_gs.ws_position = in_gs[i].ws_position;
        out_gs.color = vec4(color, 1.0);

        gl_Position = gl_in[i].gl_Position;
        EmitVertex();
    }
}
